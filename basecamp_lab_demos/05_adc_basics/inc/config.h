/**
 * This header provides configuration defaults for libsk.
 *
 * The preferred way of configuring libsk is to override current default values with user ones
 * in separate config file (or using compilation-time -DDEFINES) instead of directly modifying
 * this file.
 *
 * All defines are prefixed with SK_ and these values should be overriden instead of underscored
 * ones. I.e. specify SK_USE_SIZE_OPTIMIZATIONS, not _USE_SIZE_OPTIMIZATIONS.
 * This is done to keep this config concise
 */

/** Make library code smaller at the cost of performance penalty */
#define _USE_SIZE_OPTIMIZATIONS		0

/** Provide definitions for stuff on GL-SK board */
#define _USE_GLSK_DEFINITIONS		1


#if !defined(SK_USE_SIZE_OPTIMIZATIONS)
#define SK_USE_SIZE_OPTIMIZATIONS	(_USE_SIZE_OPTIMIZATIONS)
#endif

#if !defined(SK_USE_GLSK_DEFINITIONS)
#define SK_USE_GLSK_DEFINITIONS	(_USE_GLSK_DEFINITIONS)
#endif
