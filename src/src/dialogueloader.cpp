/*
 * $Id: dialogueloader.cpp 2819 2011-01-31 05:38:20Z darren_janeczek $
 */

#include "dialogueloader.h"
#include "error.h"

std::map<std::string, DialogueLoader *> *DialogueLoader::loaderMap = NULL;

DialogueLoader *DialogueLoader::getLoader(const std::string &mimeType) {
    zu4_assert(loaderMap != NULL, "DialogueLoader::getLoader loaderMap not initialized");
    if (loaderMap->find(mimeType) == loaderMap->end())
        return NULL;
    return (*loaderMap)[mimeType];
}

DialogueLoader *DialogueLoader::registerLoader(DialogueLoader *loader, const std::string &mimeType) {
    if (loaderMap == NULL) {
        loaderMap = new std::map<std::string, DialogueLoader *>;
    }
    (*loaderMap)[mimeType] = loader;
    return loader;
}

void DialogueLoader::cleanup() {
    if (loaderMap != NULL) {
        for (std::map<std::string, DialogueLoader *>::iterator i = loaderMap->begin(); i != loaderMap->end(); i++)
            delete i->second;
        delete loaderMap;
        loaderMap = NULL;
    }
}
