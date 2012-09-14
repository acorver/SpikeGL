#ifndef TypeDefs_H
#define TypeDefs_H

#include <qglobal.h>

typedef qint16  int16;
typedef quint16 uint16;
typedef qint64  s64;
typedef quint64 u64;
typedef s64 i64;
typedef quint32 uint32;
#ifndef HAVE_NIDAQmx
#ifndef Q_OS_WIN32
typedef qint32 int32;
#endif
#endif
typedef qint32 i32;
typedef qint32 s32;
typedef quint32 u32;
typedef double float64;

#endif
