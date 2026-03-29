#include "stats_util.h"

#include "sbuf.h"

void stats_format_label_text(char* buffer, uint32_t cap, const char* label, const char* value) {
    uint32_t len = 0;
    sbuf_reset(buffer);
    sbuf_append_str(buffer, cap, &len, label);
    sbuf_append_str(buffer, cap, &len, value);
}

void stats_format_label_dec(char* buffer, uint32_t cap, const char* label, uint32_t value) {
    uint32_t len = 0;
    sbuf_reset(buffer);
    sbuf_append_str(buffer, cap, &len, label);
    sbuf_append_dec_u32(buffer, cap, &len, value);
}

void stats_format_label_hex32(char* buffer, uint32_t cap, const char* label, uint32_t value) {
    uint32_t len = 0;
    sbuf_reset(buffer);
    sbuf_append_str(buffer, cap, &len, label);
    sbuf_append_hex_u32(buffer, cap, &len, value);
}

void stats_format_label_hex64(char* buffer, uint32_t cap, const char* label, uint64_t value) {
    uint32_t len = 0;
    sbuf_reset(buffer);
    sbuf_append_str(buffer, cap, &len, label);
    sbuf_append_hex64(buffer, cap, &len, value);
}

void stats_format_vector_line(char* buffer, uint32_t cap, uint32_t vector, const char* name) {
    uint32_t len = 0;
    sbuf_reset(buffer);
    sbuf_append_str(buffer, cap, &len, "Vector: ");
    sbuf_append_dec_u32(buffer, cap, &len, vector);
    sbuf_append_str(buffer, cap, &len, " - ");
    sbuf_append_str(buffer, cap, &len, name);
}

void stats_format_memmap_entry_line(char* buffer, uint32_t cap, uint32_t index, const char* type_name, uint64_t base, uint64_t length) {
    uint32_t len = 0;
    sbuf_reset(buffer);
    sbuf_append_str(buffer, cap, &len, " #");
    sbuf_append_dec_u32(buffer, cap, &len, index);
    sbuf_append_str(buffer, cap, &len, " ");
    sbuf_append_str(buffer, cap, &len, type_name);
    sbuf_append_str(buffer, cap, &len, " base=");
    sbuf_append_hex64(buffer, cap, &len, base);
    sbuf_append_str(buffer, cap, &len, " len=");
    sbuf_append_hex64(buffer, cap, &len, length);
}
