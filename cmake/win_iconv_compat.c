/* ZBar uses BIG-5; win-iconv 0.0.8 calls the same encoding BIG5.
 * Keep this integration shim separate from the unmodified upstream sources.
 */
#define ICONV_CONST
#include <iconv.h>
#include <errno.h>
#include <string.h>

static const char *encoding_alias(const char *name)
{
    return _stricmp(name, "BIG-5") == 0 ? "BIG5" : name;
}

iconv_t zbar1c_iconv_open(const char *to, const char *from)
{
    return iconv_open(encoding_alias(to), encoding_alias(from));
}

size_t zbar1c_iconv(iconv_t handle, char **input, size_t *input_left,
                    char **output, size_t *output_left)
{
    /* Unlike POSIX iconv, win-iconv dereferences an invalid descriptor.
     * ZBar's encoding fallback can reorder descriptors, including failed ones.
     */
    if (handle == (iconv_t)-1 || handle == NULL) {
        errno = EBADF;
        return (size_t)-1;
    }
    return iconv(handle, input, input_left, output, output_left);
}

int zbar1c_iconv_close(iconv_t handle)
{
    if (handle == (iconv_t)-1 || handle == NULL) {
        errno = EBADF;
        return -1;
    }
    return iconv_close(handle);
}
