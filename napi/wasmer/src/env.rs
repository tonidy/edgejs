use std::collections::HashMap;

use wasmer::{Memory, Table, TypedFunction};

pub(crate) const UNOFFICIAL_ENV_HANDLE: i32 = 1;

#[derive(Default)]
pub(crate) struct RuntimeEnv {
    pub(crate) memory: Option<Memory>,
    pub(crate) malloc_fn: Option<TypedFunction<i32, i32>>,
    pub(crate) table: Option<Table>,
    /// Maps value handle IDs to their guest-memory data pointers.
    /// Used for buffers/arraybuffers backed by guest linear memory.
    pub(crate) guest_data_ptrs: HashMap<u32, u32>,
}

impl RuntimeEnv {
    pub(crate) fn with_memory(memory: Memory) -> Self {
        Self {
            memory: Some(memory),
            ..Self::default()
        }
    }
}
