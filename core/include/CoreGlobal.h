#ifndef SCIQLOP_COREGLOBAL_H
#define SCIQLOP_COREGLOBAL_H

#include <QtCore/QtGlobal>

#ifdef CORE_LIB
#define SCIQLOP_CORE_EXPORT Q_DECL_EXPORT
#else
#define SCIQLOP_CORE_EXPORT Q_DECL_IMPORT
#endif

#endif // SCIQLOP_COREGLOBAL_H
