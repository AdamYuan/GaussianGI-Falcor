//
// Created by adamyuan on 2/18/25.
//

#ifndef GSGI_GS3DBOUND_HPP
#define GSGI_GS3DBOUND_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

struct GS3DBound
{
    static constexpr float kSqrt2Log255 = 3.3290429691304455f;
    static constexpr float kSqrtLog255 = 2.3539888583335364f;
    static constexpr float kSqrt2Log100 = 3.034854258770293f;
    static constexpr float kSqrtLog100 = 2.145966026289347f;
};

} // namespace GSGI

#endif // GSGI_GS3DBOUND_HPP
