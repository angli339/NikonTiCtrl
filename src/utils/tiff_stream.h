#ifndef TIFF_STREAM_H
#define TIFF_STREAM_H

#include <iostream>
#include <tiffio.h>

TIFF* TIFFStreamOpenWrite(const char* name, std::iostream *ios);
TIFF* TIFFStreamOpenRead(const char* name, std::istream *is);

#endif
