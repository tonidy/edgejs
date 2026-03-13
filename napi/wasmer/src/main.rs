use anyhow::{anyhow, bail, Context, Result};
use napi_wasmer::{run_wasix_main_capture_stdio_with_ctx, run_wasm_main_i32, GuestMount, NapiCtx};
use std::path::{Path, PathBuf};

fn init_tracing() {
    let _ = tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("warn")),
        )
        .try_init();
}

fn parse_mount(spec: &str) -> Result<GuestMount> {
    let (host, guest) = spec
        .split_once(':')
        .ok_or_else(|| anyhow!("invalid mount {spec:?}, expected <host-dir>:<guest-dir>"))?;
    let host_path = std::fs::canonicalize(host)
        .with_context(|| format!("failed to resolve host mount path {}", host))?;
    if !host_path.is_dir() {
        bail!("mount source must be a directory: {}", host_path.display());
    }
    let guest_path = PathBuf::from(guest);
    if !guest_path.is_absolute() {
        bail!(
            "mount target must be an absolute guest path: {}",
            guest_path.display()
        );
    }
    Ok(GuestMount {
        host_path,
        guest_path,
    })
}

fn main() -> Result<()> {
    init_tracing();
    let mut args = std::env::args().skip(1);
    let wasm_path = match args.next() {
        Some(p) => p,
        None => {
            bail!(
                "usage: cargo run -p napi_wasmer -- <wasm-file> [<script.js>] [--app-dir <host-dir>] [--mount <host-dir>:<guest-dir>] [wasix|main]"
            );
        }
    };
    let wasm_path = Path::new(&wasm_path);
    let mut entry = "wasix".to_string();
    let mut script_arg: Option<String> = None;
    let mut extra_mounts = Vec::new();

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "wasix" | "main" => entry = arg,
            "--app-dir" => {
                let host_dir = args
                    .next()
                    .ok_or_else(|| anyhow!("--app-dir requires a host directory"))?;
                let host_path = std::fs::canonicalize(&host_dir)
                    .with_context(|| format!("failed to resolve app dir {}", host_dir))?;
                if !host_path.is_dir() {
                    bail!("app dir must be a directory: {}", host_path.display());
                }
                extra_mounts.push(GuestMount {
                    host_path,
                    guest_path: PathBuf::from("/app"),
                });
            }
            "--mount" => {
                let spec = args
                    .next()
                    .ok_or_else(|| anyhow!("--mount requires <host-dir>:<guest-dir>"))?;
                extra_mounts.push(parse_mount(&spec)?);
            }
            _ if arg.starts_with("--mount=") => {
                extra_mounts.push(parse_mount(arg.trim_start_matches("--mount="))?);
            }
            _ if arg.starts_with("--app-dir=") => {
                let host_dir = arg.trim_start_matches("--app-dir=");
                let host_path = std::fs::canonicalize(host_dir)
                    .with_context(|| format!("failed to resolve app dir {}", host_dir))?;
                if !host_path.is_dir() {
                    bail!("app dir must be a directory: {}", host_path.display());
                }
                extra_mounts.push(GuestMount {
                    host_path,
                    guest_path: PathBuf::from("/app"),
                });
            }
            _ if script_arg.is_none() => script_arg = Some(arg),
            _ => bail!("unexpected argument: {arg}"),
        }
    }

    if entry == "wasix" {
        let napi = NapiCtx::builder().build();
        let mut guest_args = Vec::new();
        if let Some(script) = script_arg {
            let host_script = PathBuf::from(&script);
            let host_script = if host_script.is_absolute() {
                host_script
            } else {
                std::env::current_dir()
                    .context("failed to resolve current dir")?
                    .join(host_script)
            };
            let host_script = std::fs::canonicalize(&host_script)
                .with_context(|| format!("failed to resolve script {}", script))?;
            let script_parent = host_script
                .parent()
                .ok_or_else(|| anyhow!("script has no parent dir: {}", host_script.display()))?;
            if !extra_mounts
                .iter()
                .any(|m| m.guest_path == Path::new("/app"))
            {
                extra_mounts.push(GuestMount {
                    host_path: script_parent.to_path_buf(),
                    guest_path: PathBuf::from("/app"),
                });
            }
            let script_name = host_script
                .file_name()
                .ok_or_else(|| anyhow!("script has no file name: {}", host_script.display()))?;
            guest_args.push(format!("/app/{}", script_name.to_string_lossy()));
        }
        let (exit, _stdout, _stderr) =
            run_wasix_main_capture_stdio_with_ctx(&napi, wasm_path, &guest_args, &extra_mounts)?;
        println!("wasix_exit_code={exit}");
        return Ok(());
    }

    let result = run_wasm_main_i32(wasm_path)?;
    println!("main() => {result}");
    Ok(())
}
