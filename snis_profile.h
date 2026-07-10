/* snis_profile.h
 *
 * Profiler Stubs
 */
#ifndef SNIS_PROFILE_H
#define SNIS_PROFILE_H

#ifdef ENABLE_PROFILING

#include <tracy/TracyC.h>

#define PROFILE_SET_THREAD_NAME(x)	TracyCSetThreadName(x)

#define PROFILE_FRAME_START(x)		TracyCFrameMarkStart(x)
#define PROFILE_FRAME_END(x)		TracyCFrameMarkEnd(x)

#define PROFILE_ZONE_START(x)		TracyCZoneN(snis_profile_zone, x, 1)
#define PROFILE_ZONE_START_A(x, a)	TracyCZoneN(snis_profile_zone, x, a)
#define PROFILE_ZONE_END()		TracyCZoneEnd(snis_profile_zone)

#define PROFILE_ZONE_START_CTX(ctx, x)	TracyCZoneN(ctx, x, 1)
#define PROFILE_ZONE_START_A_CTX(ctx, x, a) TracyCZoneN(ctx, x, a)
#define PROFILE_ZONE_END_CTX(ctx)	TracyCZoneEnd(ctx)

#else

#define PROFILE_SET_THREAD_NAME(x)

#define PROFILE_FRAME_START(x)
#define PROFILE_FRAME_END(x)

#define PROFILE_ZONE_START(x)
#define PROFILE_ZONE_START_A(x, a)
#define PROFILE_ZONE_END(x)

#define PROFILE_ZONE_START_CTX(ctx, x)
#define PROFILE_ZONE_START_A_CTX(ctx, x, a)
#define PROFILE_ZONE_END_CTX(ctx)

#endif

#endif /* SNIS_PROFILE_H */
