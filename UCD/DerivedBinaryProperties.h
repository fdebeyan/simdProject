#ifndef DERIVEDBINARYPROPERTIES_H
#define DERIVEDBINARYPROPERTIES_H
/*
 *  Copyright (c) 2015 International Characters, Inc.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters, Inc.
 *
 *  This file is generated by UCD_properties.py - manual edits may be lost.
 */

#include "PropertyAliases.h"
#include "unicode_set.h"
#include <vector>

namespace UCD {
    namespace BIDI_M_ns {
        /** Code Point Ranges for Bidi_M
        [40, 41], [60, 60], [62, 62], [91, 91], [93, 93], [123, 123],
        [125, 125], [171, 171], [187, 187], [3898, 3901], [5787, 5788],
        [8249, 8250], [8261, 8262], [8317, 8318], [8333, 8334],
        [8512, 8512], [8705, 8708], [8712, 8717], [8721, 8721],
        [8725, 8726], [8730, 8733], [8735, 8738], [8740, 8740],
        [8742, 8742], [8747, 8755], [8761, 8761], [8763, 8780],
        [8786, 8789], [8799, 8800], [8802, 8802], [8804, 8811],
        [8814, 8844], [8847, 8850], [8856, 8856], [8866, 8867],
        [8870, 8888], [8894, 8895], [8905, 8909], [8912, 8913],
        [8918, 8941], [8944, 8959], [8968, 8971], [8992, 8993],
        [9001, 9002], [10088, 10101], [10176, 10176], [10179, 10182],
        [10184, 10185], [10187, 10189], [10195, 10198], [10204, 10206],
        [10210, 10223], [10627, 10648], [10651, 10671], [10680, 10680],
        [10688, 10693], [10697, 10697], [10702, 10706], [10708, 10709],
        [10712, 10716], [10721, 10721], [10723, 10725], [10728, 10729],
        [10740, 10745], [10748, 10749], [10762, 10780], [10782, 10785],
        [10788, 10788], [10790, 10790], [10793, 10793], [10795, 10798],
        [10804, 10805], [10812, 10814], [10839, 10840], [10852, 10853],
        [10858, 10861], [10863, 10864], [10867, 10868], [10873, 10915],
        [10918, 10925], [10927, 10966], [10972, 10972], [10974, 10974],
        [10978, 10982], [10988, 10990], [10995, 10995], [10999, 11003],
        [11005, 11005], [11778, 11781], [11785, 11786], [11788, 11789],
        [11804, 11805], [11808, 11817], [12296, 12305], [12308, 12315],
        [65113, 65118], [65124, 65125], [65288, 65289], [65308, 65308],
        [65310, 65310], [65339, 65339], [65341, 65341], [65371, 65371],
        [65373, 65373], [65375, 65376], [65378, 65379], [120539, 120539],
        [120597, 120597], [120655, 120655], [120713, 120713],
        [120771, 120771]**/
        const UnicodeSet codepoint_set 
            {{{Empty, 1}, {Mixed, 3}, {Empty, 1}, {Mixed, 1}, {Empty, 115},
              {Mixed, 1}, {Empty, 58}, {Mixed, 1}, {Empty, 76}, {Mixed, 4},
              {Empty, 5}, {Mixed, 1}, {Empty, 5}, {Mixed, 10}, {Empty, 33},
              {Mixed, 1}, {Empty, 2}, {Mixed, 2}, {Empty, 12}, {Mixed, 8},
              {Full, 1}, {Mixed, 3}, {Empty, 24}, {Mixed, 2}, {Empty, 14},
              {Mixed, 1}, {Empty, 1649}, {Mixed, 2}, {Empty, 4}, {Mixed, 4},
              {Empty, 1722}, {Mixed, 1}, {Empty, 1}, {Mixed, 1}, {Empty, 1},
              {Mixed, 1}, {Empty, 1}, {Mixed, 1}, {Empty, 1}, {Mixed, 1},
              {Empty, 31041}},
             {0x50000300, 0x28000000, 0x28000000, 0x08000800, 0x3c000000,
              0x18000000, 0x06000000, 0x00000060, 0x60000000, 0x00006000,
              0x00000001, 0xbc623f1e, 0xfa0ff857, 0x803c1fff, 0xffffcff5,
              0x01079fff, 0xc1ffffcc, 0xffc33e00, 0xffff3fff, 0x00000f00,
              0x00000603, 0x003fff00, 0x70783b79, 0x0000fffc, 0xf9fffff8,
              0x0100ffff, 0x1f37c23f, 0x33f0033a, 0xdffffc00, 0x70307a53,
              0x01800000, 0xfe19bc30, 0xffffbfcf, 0x507fffff, 0x2f88707c,
              0x3000363c, 0x000003ff, 0x0ff3ff00, 0x7e000000, 0x00000030,
              0x50000300, 0x28000000, 0xa8000000, 0x0000000d, 0x08000000,
              0x00200000, 0x00008000, 0x00000200, 0x00000008}};
        static BinaryPropertyObject property_object{Bidi_M, codepoint_set};
    }
}


#endif
