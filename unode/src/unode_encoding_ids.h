#ifndef UNODE_ENCODING_IDS_H_
#define UNODE_ENCODING_IDS_H_

#include <cstdint>

namespace unode::encoding_ids {

constexpr int32_t kEncAscii = 0;
constexpr int32_t kEncUtf8 = 1;
constexpr int32_t kEncBase64 = 2;
constexpr int32_t kEncBase64Url = 3;
constexpr int32_t kEncUtf16Le = 4;
constexpr int32_t kEncHex = 5;
constexpr int32_t kEncBuffer = 6;
constexpr int32_t kEncLatin1 = 7;

}  // namespace unode::encoding_ids

#endif  // UNODE_ENCODING_IDS_H_
