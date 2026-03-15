/*
 * nspire_xml_shim.h - Minimal libxml2 replacement for TI-Nspire
 *
 * Provides a lightweight DOM tree built with yxml SAX parser.
 * Supports the subset of libxml2 API used by zu4:
 *   - xmlParseFile, xmlFreeDoc
 *   - xmlGetProp, xmlHasProp, xmlFree
 *   - xmlStrcmp, xmlStrcasecmp
 *   - XPath queries (simple /config/name paths)
 *   - XInclude processing
 *   - Config file I/O callbacks
 */

#ifndef NSPIRE_XML_SHIM_H
#define NSPIRE_XML_SHIM_H

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* === Types matching libxml2 API === */

typedef unsigned char xmlChar;

/* XML Node types */
#define XML_ELEMENT_NODE    1
#define XML_TEXT_NODE       3
#define XML_DOCUMENT_NODE   9

/* XML Node - simplified DOM node */
typedef struct _xmlNode {
    int type;
    const xmlChar *name;        /* element name (owned) */
    struct _xmlNode *children;  /* first child */
    struct _xmlNode *next;      /* next sibling */
    struct _xmlNode *parent;    /* parent node */
    struct _xmlAttr *properties; /* attribute list */
} xmlNode;
typedef xmlNode *xmlNodePtr;

/* XML Attribute */
typedef struct _xmlAttr {
    const xmlChar *name;        /* attribute name (owned) */
    const xmlChar *value;       /* attribute value (owned) */
    struct _xmlAttr *next;      /* next attribute */
} xmlAttr;
typedef xmlAttr *xmlAttrPtr;

/* XML Document */
typedef struct _xmlDoc {
    int type;
    xmlNodePtr children;        /* root element */
    void *intSubset;            /* DTD (always NULL for us) */
} xmlDoc;
typedef xmlDoc *xmlDocPtr;

/* XPath */
typedef struct _xmlNodeSet {
    int nodeNr;
    int nodeMax;
    xmlNodePtr *nodeTab;
} xmlNodeSet;
typedef xmlNodeSet *xmlNodeSetPtr;

typedef struct _xmlXPathObject {
    xmlNodeSetPtr nodesetval;
} xmlXPathObject;
typedef xmlXPathObject *xmlXPathObjectPtr;

typedef struct _xmlXPathContext {
    xmlDocPtr doc;
} xmlXPathContext;
typedef xmlXPathContext *xmlXPathContextPtr;

/* Validation context (stubbed) */
typedef struct _xmlValidCtxt {
    void *userData;
    void (*error)(void *, const char *, ...);
} xmlValidCtxt;
typedef xmlValidCtxt *xmlValidCtxtPtr;

/* Additional node types */
#define XML_COMMENT_NODE    8

/* Macros */
#define xmlXPathNodeSetIsEmpty(ns) (((ns) == NULL) || ((ns)->nodeNr == 0))
#define BAD_CAST (xmlChar *)

/* Compatibility: xmlChildrenNode is an alias for children in libxml2 */
#define xmlChildrenNode children

/* === Core API === */

/* Parsing */
xmlDocPtr xmlParseFile(const char *filename);
void xmlFreeDoc(xmlDocPtr doc);

/* Node properties */
xmlChar *xmlGetProp(xmlNodePtr node, const xmlChar *name);
xmlAttrPtr xmlHasProp(xmlNodePtr node, const xmlChar *name);
void xmlFree(void *ptr);

/* String utilities */
int xmlStrcmp(const xmlChar *a, const xmlChar *b);
int xmlStrcasecmp(const xmlChar *a, const xmlChar *b);

/* XPath */
xmlXPathContextPtr xmlXPathNewContext(xmlDocPtr doc);
xmlXPathObjectPtr xmlXPathEvalExpression(const xmlChar *expr, xmlXPathContextPtr ctx);
void xmlXPathFreeContext(xmlXPathContextPtr ctx);
void xmlXPathFreeObject(xmlXPathObjectPtr obj);

/* XInclude */
int xmlXIncludeProcess(xmlDocPtr doc);

/* Validation (stubbed) */
int xmlValidateDocument(xmlValidCtxtPtr cvp, xmlDocPtr doc);

/* Additional utility functions */
xmlNodePtr xmlDocGetRootElement(xmlDocPtr doc);
int xmlNodeIsText(xmlNodePtr node);
xmlChar *xmlNodeGetContent(xmlNodePtr node);

/* I/O callbacks */
typedef int (*xmlInputMatchCallback)(const char *);
typedef void *(*xmlInputOpenCallback)(const char *);
typedef int (*xmlInputReadCallback)(void *, char *, int);
typedef int (*xmlInputCloseCallback)(void *);

void xmlRegisterInputCallbacks(xmlInputMatchCallback matchFunc,
                               xmlInputOpenCallback openFunc,
                               xmlInputReadCallback readFunc,
                               xmlInputCloseCallback closeFunc);

int xmlFileMatch(const char *filename);
void *xmlFileOpen(const char *filename);
int xmlFileRead(void *context, char *buffer, int len);
int xmlFileClose(void *context);

#ifdef __cplusplus
}
#endif

#endif /* NSPIRE_XML_SHIM_H */
