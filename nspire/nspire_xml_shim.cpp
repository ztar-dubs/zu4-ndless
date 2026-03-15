/*
 * nspire_xml_shim.cpp - Minimal libxml2 replacement using yxml
 *
 * Builds a DOM tree from XML files using the yxml SAX parser.
 * Supports simple XPath queries like "/config/tilesets" and XInclude.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include <string>

extern "C" {
#include <yxml.h>
}

#include "nspire_xml_shim.h"

/* I/O callback storage */
static xmlInputMatchCallback io_match = NULL;
static xmlInputOpenCallback io_open = NULL;
static xmlInputReadCallback io_read = NULL;
static xmlInputCloseCallback io_close = NULL;

/* Debug logging (disabled) */
static void dbg(const char *fmt, ...) {
    (void)fmt;
}

/* ========== Internal helpers ========== */

static xmlChar *xmlStrdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    xmlChar *r = (xmlChar *)malloc(len);
    memcpy(r, s, len);
    return r;
}

static xmlNodePtr xmlNewNode(const char *name, int type) {
    xmlNodePtr node = (xmlNodePtr)calloc(1, sizeof(xmlNode));
    node->type = type;
    node->name = xmlStrdup(name);
    return node;
}

static void xmlAddChild(xmlNodePtr parent, xmlNodePtr child) {
    child->parent = parent;
    if (!parent->children) {
        parent->children = child;
    } else {
        xmlNodePtr last = parent->children;
        while (last->next) last = last->next;
        last->next = child;
    }
}

static void xmlAddAttr(xmlNodePtr node, const char *name, const char *value) {
    xmlAttrPtr attr = (xmlAttrPtr)calloc(1, sizeof(xmlAttr));
    attr->name = xmlStrdup(name);
    attr->value = xmlStrdup(value);
    attr->next = node->properties;
    node->properties = attr;
}

static void xmlFreeNode(xmlNodePtr node) {
    if (!node) return;
    xmlFreeNode(node->children);
    xmlFreeNode(node->next);

    /* Free attributes */
    xmlAttrPtr attr = node->properties;
    while (attr) {
        xmlAttrPtr next = attr->next;
        free((void *)attr->name);
        free((void *)attr->value);
        free(attr);
        attr = next;
    }
    free((void *)node->name);
    free(node);
}

/* ========== yxml-based XML parser ========== */

static char *read_file(const char *filename, long *outlen) {
    char *data = NULL;

    dbg("read_file('%s') io_callbacks=%s\n", filename,
        (io_match && io_open && io_read && io_close) ? "yes" : "no");

    /* Try custom I/O callbacks first */
    if (io_match && io_open && io_read && io_close) {
        if (io_match(filename)) {
            void *ctx = io_open(filename);
            dbg("  io_open('%s') -> %p\n", filename, ctx);
            if (ctx) {
                /* Read in chunks */
                size_t capacity = 4096;
                size_t total = 0;
                data = (char *)malloc(capacity);
                int n;
                while ((n = io_read(ctx, data + total, (int)(capacity - total - 1))) > 0) {
                    total += n;
                    if (total + 1 >= capacity) {
                        capacity *= 2;
                        data = (char *)realloc(data, capacity);
                    }
                }
                data[total] = '\0';
                *outlen = (long)total;
                io_close(ctx);
                return data;
            }
        }
    }

    /* Fallback to stdio (fopen is overridden on NSPIRE to try .tns) */
    dbg("  fallback fopen('%s')\n", filename);
    FILE *f = fopen(filename, "rb");
    dbg("  fallback fopen('%s') -> %p\n", filename, (void*)f);
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    data = (char *)malloc(len + 1);
    if (!data) { fclose(f); return NULL; }

    fread(data, 1, len, f);
    data[len] = '\0';
    *outlen = len;
    fclose(f);
    return data;
}

static xmlDocPtr parse_xml_data(const char *data, long len, const char *basepath);

static xmlDocPtr parse_xml_file_internal(const char *filename, const char *basepath) {
    long len = 0;
    char *data = read_file(filename, &len);
    if (!data) return NULL;

    /* Determine base path for XInclude resolution */
    char newbase[256] = "";
    if (basepath) {
        strncpy(newbase, basepath, sizeof(newbase) - 1);
    } else {
        strncpy(newbase, filename, sizeof(newbase) - 1);
        char *last_slash = strrchr(newbase, '/');
        if (last_slash) *(last_slash + 1) = '\0';
        else newbase[0] = '\0';
    }

    xmlDocPtr doc = parse_xml_data(data, len, newbase);
    free(data);
    return doc;
}

static void flush_text_buf(xmlNodePtr current, char *text_buf, int &text_len) {
    if (text_len > 0 && current) {
        text_buf[text_len] = '\0';
        xmlNodePtr tn = xmlNewNode(text_buf, XML_TEXT_NODE);
        xmlAddChild(current, tn);
        text_len = 0;
        text_buf[0] = '\0';
    }
}

static xmlDocPtr parse_xml_data(const char *data, long len, const char *basepath) {
    #define YXML_BUFSIZE 2048
    char yxml_buf[YXML_BUFSIZE];
    yxml_t x;
    yxml_init(&x, yxml_buf, YXML_BUFSIZE);

    xmlDocPtr doc = (xmlDocPtr)calloc(1, sizeof(xmlDoc));
    doc->type = XML_DOCUMENT_NODE;
    doc->intSubset = NULL;

    xmlNodePtr root = NULL;
    xmlNodePtr current = NULL;

    /* Temporary buffers for attribute and text parsing */
    char attr_name[256] = "";
    char attr_val[1024] = "";
    int attr_val_len = 0;
    #define TEXT_BUFSIZE 4096
    char text_buf[TEXT_BUFSIZE] = "";
    int text_len = 0;

    for (long i = 0; i < len; i++) {
        yxml_ret_t r = yxml_parse(&x, data[i]);
        if (r < 0) break; /* parse error */

        switch (r) {
        case YXML_ELEMSTART: {
            /* Flush accumulated text as a text node */
            flush_text_buf(current, text_buf, text_len);

            xmlNodePtr node = xmlNewNode(x.elem, XML_ELEMENT_NODE);
            if (!root) {
                root = node;
                doc->children = root;
            }
            if (current) {
                xmlAddChild(current, node);
            }
            current = node;
            break;
        }

        case YXML_ELEMEND: {
            /* Flush accumulated text as a text node */
            flush_text_buf(current, text_buf, text_len);

            if (current && current->parent) {
                current = current->parent;
            }
            break;
        }

        case YXML_CONTENT: {
            /* Accumulate text content character by character */
            if (text_len < TEXT_BUFSIZE - 1) {
                text_buf[text_len++] = *x.data;
            }
            break;
        }

        case YXML_ATTRSTART: {
            strncpy(attr_name, x.attr, sizeof(attr_name) - 1);
            attr_name[sizeof(attr_name) - 1] = '\0';
            attr_val[0] = '\0';
            attr_val_len = 0;
            break;
        }

        case YXML_ATTRVAL: {
            if (attr_val_len < (int)sizeof(attr_val) - 1) {
                attr_val[attr_val_len++] = *x.data;
                attr_val[attr_val_len] = '\0';
            }
            break;
        }

        case YXML_ATTREND: {
            if (current) {
                xmlAddAttr(current, attr_name, attr_val);
            }
            break;
        }

        default:
            break;
        }
    }

    return doc;
}

/* ========== Public API ========== */

extern "C" {

xmlDocPtr xmlParseFile(const char *filename) {
    dbg("xmlParseFile('%s')\n", filename);
    xmlDocPtr doc = parse_xml_file_internal(filename, NULL);
    dbg("xmlParseFile('%s') -> %s\n", filename, doc ? "OK" : "NULL");
    return doc;
}

void xmlFreeDoc(xmlDocPtr doc) {
    if (!doc) return;
    xmlFreeNode(doc->children);
    free(doc);
}

xmlChar *xmlGetProp(xmlNodePtr node, const xmlChar *name) {
    if (!node || !name) return NULL;
    for (xmlAttrPtr attr = node->properties; attr; attr = attr->next) {
        if (strcmp((const char *)attr->name, (const char *)name) == 0) {
            return xmlStrdup((const char *)attr->value);
        }
    }
    return NULL;
}

xmlAttrPtr xmlHasProp(xmlNodePtr node, const xmlChar *name) {
    if (!node || !name) return NULL;
    for (xmlAttrPtr attr = node->properties; attr; attr = attr->next) {
        if (strcmp((const char *)attr->name, (const char *)name) == 0)
            return attr;
    }
    return NULL;
}

void xmlFree(void *ptr) {
    free(ptr);
}

int xmlStrcmp(const xmlChar *a, const xmlChar *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp((const char *)a, (const char *)b);
}

int xmlStrcasecmp(const xmlChar *a, const xmlChar *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    const char *sa = (const char *)a;
    const char *sb = (const char *)b;
    while (*sa && *sb) {
        int ca = tolower((unsigned char)*sa);
        int cb = tolower((unsigned char)*sb);
        if (ca != cb) return ca - cb;
        sa++; sb++;
    }
    return tolower((unsigned char)*sa) - tolower((unsigned char)*sb);
}

/* Simple XPath: supports paths like "/config/tilesets" */
static void xpath_collect(xmlNodePtr node, const char **parts, int depth, int total,
                          xmlNodePtr *results, int *count, int max_results) {
    if (!node || depth >= total || *count >= max_results) return;

    for (xmlNodePtr child = node->children; child; child = child->next) {
        if (child->type != XML_ELEMENT_NODE) continue;
        if (strcmp((const char *)child->name, parts[depth]) == 0) {
            if (depth == total - 1) {
                results[(*count)++] = child;
            } else {
                xpath_collect(child, parts, depth + 1, total, results, count, max_results);
            }
        }
    }
}

xmlXPathContextPtr xmlXPathNewContext(xmlDocPtr doc) {
    xmlXPathContextPtr ctx = (xmlXPathContextPtr)calloc(1, sizeof(xmlXPathContext));
    ctx->doc = doc;
    return ctx;
}

xmlXPathObjectPtr xmlXPathEvalExpression(const xmlChar *expr, xmlXPathContextPtr ctx) {
    xmlXPathObjectPtr obj = (xmlXPathObjectPtr)calloc(1, sizeof(xmlXPathObject));
    obj->nodesetval = (xmlNodeSetPtr)calloc(1, sizeof(xmlNodeSet));
    obj->nodesetval->nodeMax = 64;
    obj->nodesetval->nodeTab = (xmlNodePtr *)calloc(64, sizeof(xmlNodePtr));
    obj->nodesetval->nodeNr = 0;

    /* Parse the XPath expression (simple paths only) */
    const char *path = (const char *)expr;
    if (path[0] != '/') return obj;

    /* Split path into parts */
    const char *parts[16];
    int nparts = 0;
    char pathcopy[256];
    strncpy(pathcopy, path + 1, sizeof(pathcopy) - 1); /* skip leading / */
    pathcopy[sizeof(pathcopy) - 1] = '\0';

    char *tok = strtok(pathcopy, "/");
    while (tok && nparts < 16) {
        parts[nparts++] = tok;
        tok = strtok(NULL, "/");
    }

    if (nparts > 0 && ctx->doc && ctx->doc->children) {
        /* First part must match the root element */
        xmlNodePtr root = ctx->doc->children;
        if (root && strcmp((const char *)root->name, parts[0]) == 0) {
            if (nparts == 1) {
                obj->nodesetval->nodeTab[0] = root;
                obj->nodesetval->nodeNr = 1;
            } else {
                xpath_collect(root, parts, 1, nparts,
                             obj->nodesetval->nodeTab,
                             &obj->nodesetval->nodeNr,
                             obj->nodesetval->nodeMax);
            }
        }
    }

    return obj;
}

void xmlXPathFreeContext(xmlXPathContextPtr ctx) {
    free(ctx);
}

void xmlXPathFreeObject(xmlXPathObjectPtr obj) {
    if (!obj) return;
    if (obj->nodesetval) {
        free(obj->nodesetval->nodeTab);
        free(obj->nodesetval);
    }
    free(obj);
}

/* XInclude: process <xi:include href="..."/> or <include href="..."/> elements */
static void process_xinclude_node(xmlNodePtr parent, const char *basepath) {
    xmlNodePtr child = parent->children;
    while (child) {
        xmlNodePtr next = child->next;

        const char *nodename = (const char *)child->name;
        /* Match xi:include or include */
        if (child->type == XML_ELEMENT_NODE &&
            (strcmp(nodename, "xi:include") == 0 || strcmp(nodename, "include") == 0)) {
            xmlChar *href = xmlGetProp(child, (const xmlChar *)"href");
            if (href) {
                char filepath[256];
                snprintf(filepath, sizeof(filepath), "%s%s", basepath ? basepath : "", (const char *)href);
                dbg("XInclude: '%s'\n", filepath);
                xmlFree(href);

                xmlDocPtr inc = parse_xml_file_internal(filepath, basepath);
                if (inc && inc->children) {
                    /* Replace the <xi:include> node with the included document's root element */
                    xmlNodePtr inc_root = inc->children;

                    /* Detach inc_root from the document */
                    inc->children = NULL;

                    /* Re-parent inc_root into current tree */
                    inc_root->parent = parent;
                    inc_root->next = next;

                    /* Replace child with inc_root in parent's child list */
                    if (child == parent->children) {
                        parent->children = inc_root;
                    } else {
                        xmlNodePtr prev = parent->children;
                        while (prev && prev->next != child) prev = prev->next;
                        if (prev) prev->next = inc_root;
                    }

                    /* Free the old xi:include node (just the node, not siblings) */
                    child->next = NULL;
                    xmlFreeNode(child);

                    /* Free the now-empty document shell */
                    xmlFreeDoc(inc);

                    dbg("XInclude: inserted <%s>\n", (const char *)inc_root->name);

                    /* Recurse into the included element */
                    process_xinclude_node(inc_root, basepath);
                } else {
                    if (inc) xmlFreeDoc(inc);
                    dbg("XInclude: FAILED to load '%s'\n", filepath);
                }
            }
        } else {
            /* Recurse */
            process_xinclude_node(child, basepath);
        }
        child = next;
    }
}

int xmlXIncludeProcess(xmlDocPtr doc) {
    if (!doc || !doc->children) return 0;

    /* Determine base path from document */
    /* We use a simple heuristic - basepath should have been set during parsing */
    process_xinclude_node(doc->children, "conf/");
    return 0;
}

int xmlValidateDocument(xmlValidCtxtPtr cvp, xmlDocPtr doc) {
    /* Validation is not supported - always return success */
    (void)cvp;
    (void)doc;
    return 1;
}

void xmlRegisterInputCallbacks(xmlInputMatchCallback matchFunc,
                               xmlInputOpenCallback openFunc,
                               xmlInputReadCallback readFunc,
                               xmlInputCloseCallback closeFunc) {
    io_match = matchFunc;
    io_open = openFunc;
    io_read = readFunc;
    io_close = closeFunc;
}

int xmlFileMatch(const char *filename) {
    return (filename != NULL);
}

void *xmlFileOpen(const char *filename) {
    FILE *f = fopen(filename, "rb");
    dbg("xmlFileOpen('%s') -> %p\n", filename, (void*)f);
    return (void *)f;
}

int xmlFileRead(void *context, char *buffer, int len) {
    return (int)fread(buffer, 1, len, (FILE *)context);
}

int xmlFileClose(void *context) {
    return fclose((FILE *)context);
}

xmlNodePtr xmlDocGetRootElement(xmlDocPtr doc) {
    if (!doc) return NULL;
    return doc->children;
}

int xmlNodeIsText(xmlNodePtr node) {
    if (!node) return 0;
    return (node->type == XML_TEXT_NODE) ? 1 : 0;
}

xmlChar *xmlNodeGetContent(xmlNodePtr node) {
    /* For text nodes, return a copy of the name (which stores text content) */
    /* For element nodes, concatenate text children */
    if (!node) return NULL;

    if (node->type == XML_TEXT_NODE && node->name) {
        return xmlStrdup((const char *)node->name);
    }

    /* For element nodes, find text child */
    std::string content;
    for (xmlNodePtr child = node->children; child; child = child->next) {
        if (child->type == XML_TEXT_NODE && child->name) {
            content += (const char *)child->name;
        }
    }
    if (content.empty()) return NULL;
    return xmlStrdup(content.c_str());
}

} /* extern "C" */
