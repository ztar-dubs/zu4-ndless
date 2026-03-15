/* Redirect libxml2 includes to our minimal shim */
#ifndef NSPIRE_LIBXML_XINCLUDE_H
#define NSPIRE_LIBXML_XINCLUDE_H
#include "nspire_xml_shim.h"

/* Stub out the xinclude requirement check */
#define LIBXML_XINCLUDE_ENABLED 1
#endif
