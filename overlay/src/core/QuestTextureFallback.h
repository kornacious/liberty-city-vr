#pragma once

// Desktop re3 installs this search fallback with EXTENDED_PIPELINES. Quest
// still needs the shared generic dictionary even without desktop Neo pipes.
namespace QuestTextureFallback {
static rw::Texture *(*previousFind)(const char *);
static rw::TexDictionary *generic;

static rw::Texture *Find(const char *name)
{
    rw::Texture *texture = previousFind ? previousFind(name) : nil;
    return texture || !generic ? texture : generic->find(name);
}

static void Install(rw::TexDictionary *dictionary)
{
    if(rw::Texture::findCB != Find) previousFind = rw::Texture::findCB;
    generic = dictionary;
    rw::Texture::findCB = Find;
}

static void Remove()
{
    if(rw::Texture::findCB == Find) rw::Texture::findCB = previousFind;
    generic = nil;
    previousFind = nil;
}
}
