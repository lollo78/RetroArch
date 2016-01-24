/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_archive.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef FILE_ARCHIVE_H__
#define FILE_ARCHIVE_H__

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>

enum zlib_transfer_type
{
   ZLIB_TRANSFER_NONE = 0,
   ZLIB_TRANSFER_INIT,
   ZLIB_TRANSFER_ITERATE,
   ZLIB_TRANSFER_DEINIT,
   ZLIB_TRANSFER_DEINIT_ERROR
};

struct zlib_file_backend
{
   void *(*stream_new)(void);
   void  (*stream_free)(void *);
   uint32_t (*stream_get_avail_in)(void*);
   uint32_t (*stream_get_avail_out)(void*);
   uint64_t (*stream_get_total_out)(void*);
   void     (*stream_decrement_total_out)(void *, unsigned);
   const char *ident;
};

typedef struct zlib_transfer
{
   void *handle;
   const uint8_t *footer;
   const uint8_t *directory;
   const uint8_t *data;
   int32_t zip_size;
   enum zlib_transfer_type type;
   const struct zlib_file_backend *backend;
} zlib_transfer_t;

typedef struct zlib_handle
{
   void     *stream;
   uint8_t *data;
   uint32_t real_checksum;
   const struct zlib_file_backend *backend;
} zlib_file_handle_t;

/* Returns true when parsing should continue. False to stop. */
typedef int (*file_archive_file_cb)(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata);

uint32_t file_archive_crc32_calculate(uint32_t crc, const uint8_t *data, size_t length);

/**
 * zlib_parse_file:
 * @file                        : filename path of archive
 * @valid_exts                  : Valid extensions of archive to be parsed. 
 *                                If NULL, allow all.
 * @file_cb                     : file_cb function pointer
 * @userdata                    : userdata to pass to file_cb function pointer.
 *
 * Low-level file parsing. Enumerates over all files and calls 
 * file_cb with userdata.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool file_archive_parse_file(const char *file, const char *valid_exts,
      file_archive_file_cb file_cb, void *userdata);

int file_archive_parse_file_iterate(
      zlib_transfer_t *state,
      bool *returnerr,
      const char *file,
      const char *valid_exts,
      file_archive_file_cb file_cb,
      void *userdata);

void file_archive_parse_file_iterate_stop(zlib_transfer_t *state);

int file_archive_parse_file_progress(zlib_transfer_t *state);

/**
 * file_archive_extract_first_content_file:
 * @zip_path                    : filename path to ZIP archive.
 * @zip_path_size               : size of ZIP archive.
 * @valid_exts                  : valid extensions for a content file.
 * @extraction_directory        : the directory to extract temporary
 *                                unzipped content to.
 *
 * Extract first content file from archive.
 *
 * Returns : true (1) on success, otherwise false (0).
 **/
bool file_archive_extract_first_content_file(char *zip_path, size_t zip_path_size, 
      const char *valid_exts, const char *extraction_dir,
      char *out_path, size_t len);

/**
 * file_archive_get_file_list:
 * @path                        : filename path of archive
 * @valid_exts                  : Valid extensions of archive to be parsed. 
 *                                If NULL, allow all.
 *
 * Returns: string listing of files from archive on success, otherwise NULL.
 **/
struct string_list *file_archive_get_file_list(const char *path, const char *valid_exts);

bool file_archive_inflate_data_to_file_init(
      zlib_file_handle_t *handle,
      const uint8_t *cdata,  uint32_t csize, uint32_t size);

int file_archive_inflate_data_to_file_iterate(void *data);

/**
 * file_archive_inflate_data_to_file:
 * @path                        : filename path of archive.
 * @cdata                       : input data.
 * @csize                       : size of input data.
 * @size                        : output file size
 * @checksum                    : CRC32 checksum from input data.
 *
 * Decompress data to file.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
int file_archive_inflate_data_to_file(zlib_file_handle_t *handle,
      int ret, const char *path, const char *valid_exts,
      const uint8_t *cdata, uint32_t csize, uint32_t size, uint32_t checksum);

bool file_archive_perform_mode(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata);

struct string_list *compressed_file_list_new(const char *filename,
      const char* ext);


void zlib_deflate_init(void *data, int level);

int zlib_deflate_data_to_file(void *data);

void zlib_stream_deflate_free(void *data);

bool file_archive_inflate_init(void *data);

void zlib_set_stream(void *data,
      uint32_t       avail_in,
      uint32_t       avail_out,
      const uint8_t *next_in,
      uint8_t       *next_out
      );

const struct zlib_file_backend *file_archive_get_default_file_backend(void);

const struct zlib_file_backend zlib_backend;

#endif
