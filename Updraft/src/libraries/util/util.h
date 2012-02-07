#ifndef UPDRAFT_SRC_LIBRARIES_UTIL_UTIL_H_
#define UPDRAFT_SRC_LIBRARIES_UTIL_UTIL_H_

#include <QtGlobal>

#ifdef UPDRAFT_UTIL_INTERNAL
  #define UTIL_EXPORT Q_DECL_EXPORT
#else
  #define UTIL_EXPORT Q_DECL_IMPORT
#endif

#include "location.h"
#include "units.h"

#endif  // UPDRAFT_SRC_LIBRARIES_UTIL_UTIL_H_