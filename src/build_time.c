/* Glue to expose the assembler-generated build_time_str to C code */

extern const char build_time_str[];

const char *get_build_time(void)
{
    return build_time_str;
}
