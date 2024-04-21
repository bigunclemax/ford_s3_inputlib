#ifndef SDL_internal_h_
#define SDL_internal_h_

#include <SDL3/SDL.h>


#define SDL_SwapLE16(X)	 (X)

#define SDL_malloc malloc
#define SDL_calloc calloc
#define SDL_realloc realloc
#define SDL_free free
#ifndef SDL_memcpy
#define SDL_memcpy memcpy
#endif
#ifndef SDL_memmove
#define SDL_memmove memmove
#endif
#ifndef SDL_memset
#define SDL_memset memset
#endif
#define SDL_memcmp memcmp
#define SDL_strlcpy strlcpy
#define SDL_strlcat strlcat
#define SDL_strlen strlen
#define SDL_wcslen wcslen
#define SDL_wcslcpy wcslcpy
#define SDL_wcslcat wcslcat
#define SDL_strdup strdup
#define SDL_wcsdup wcsdup
#define SDL_strchr strchr
#define SDL_strrchr strrchr
#define SDL_strstr strstr
#define SDL_wcsstr wcsstr
#define SDL_strtok_r strtok_r
#define SDL_strcmp strcmp
#define SDL_wcscmp wcscmp
#define SDL_strncmp strncmp
#define SDL_wcsncmp wcsncmp
#define SDL_strcasecmp strcasecmp
#define SDL_strncasecmp strncasecmp
#define SDL_sscanf sscanf
#define SDL_vsscanf vsscanf
#define SDL_snprintf snprintf
#define SDL_vsnprintf vsnprintf

#define SDL_atoi atoi
#define SDL_strtol strtol

#define SDL_ASSERT_CAPABILITY(x)
#define SDL_GUARDED_BY(x)
#define SDL_assert(x)

typedef enum
{
    SDL_SENSOR_INVALID = -1,    /**< Returned for an invalid sensor */
    SDL_SENSOR_UNKNOWN,         /**< Unknown sensor type */
    SDL_SENSOR_ACCEL,           /**< Accelerometer */
    SDL_SENSOR_GYRO,            /**< Gyroscope */
    SDL_SENSOR_ACCEL_L,         /**< Accelerometer for left Joy-Con controller and Wii nunchuk */
    SDL_SENSOR_GYRO_L,          /**< Gyroscope for left Joy-Con controller */
    SDL_SENSOR_ACCEL_R,         /**< Accelerometer for right Joy-Con controller */
    SDL_SENSOR_GYRO_R           /**< Gyroscope for right Joy-Con controller */
} SDL_SensorType;

#endif /* SDL_internal_h_ */