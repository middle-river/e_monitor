// CCITT Group-4 decoder for the TIFF file format.
// 2022-05-27  T. Nakagawa

#ifndef TIFF_G4_H_
#define TIFF_G4_H_

#include <stdint.h>
#include <functional>

bool tiffg4_decoder(const uint8_t *file, int width, int height, std::function<void(const uint8_t *, int)> callback);

#endif
