/** @file resourcesystem.h  Resource subsystem.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef DENG_RESOURCESYSTEM_H
#define DENG_RESOURCESYSTEM_H

#include "def_data.h"
#include "resourceclass.h"
#include "resource/animgroup.h"
#include "resource/colorpalette.h"
#ifdef __CLIENT__
#  include "Fonts"
#endif
#include "Materials"
#include "resource/sprite.h"
#include "Textures"
#include <de/Error>
#include <de/String>
#include <de/System>

/**
 * Logical resources; materials, packages, textures, etc...
 *
 * @par Textures
 *
 * @em Clearing a texture is to 'undefine' it - any names bound to it will be
 * deleted and any GL textures acquired for it are 'released'. The logical
 * Texture instance used to represent it is also deleted.
 *
 * @em Releasing a texture will leave it defined (any names bound to it will
 * persist) but any GL textures acquired for it are 'released'. Note that the
 * logical Texture instance used to represent is NOT be deleted.
 *
 * @par Fonts
 *
 * @em Runtime fonts are not loaded until precached or actually needed. They
 * may be cleared, in which case they will be reloaded when needed.
 *
 * @em System fonts are loaded at startup and remain in memory all the time.
 * After clearing they must be manually reloaded.
 *
 * @ingroup resource
 */
class ResourceSystem : public de::System
{
public:
    /// An unknown resource class identifier was specified. @ingroup errors
    DENG2_ERROR(UnknownResourceClassError);

    /// The referenced color palette could not be found. @ingroup errors
    DENG2_ERROR(MissingColorPaletteError);

    /// The referenced sprite could not be found. @ingroup errors
    DENG2_ERROR(MissingSpriteError);

    typedef QList<Sprite *> SpriteSet;
public:
    /**
     * Construct a new resource system, configuring all resource classes and
     * their collections.
     */
    ResourceSystem();

    /**
     * Register the console commands, variables, etc..., of this module.
     */
    static void consoleRegister();

    // System.
    void timeChanged(de::Clock const &);

    /**
     * Lookup a ResourceClass by symbolic @a name.
     */
    de::ResourceClass &resClass(de::String name);

    /**
     * Lookup a ResourceClass by @a id.
     * @todo Refactor away.
     */
    de::ResourceClass &resClass(resourceclassid_t id);

    void clearAllRuntimeResources();
    void clearAllSystemResources();

    /**
     * Provides access to the Materials collection.
     */
    de::Materials &materials();

    /**
     * Builds the sprite rotation matrixes to account for horizontally flipped
     * sprites.  Will report an error if the lumps are inconsistant.
     *
     * Sprite lump names are 4 characters for the actor, a letter for the frame,
     * and a number for the rotation, A sprite that is flippable will have an
     * additional letter/number appended.  The rotation character can be 0 to
     * signify no rotations.
     */
    void initSprites();

    /**
     * Destroys all sprites in all sprite sets.
     */
    void clearAllSprites();

    /**
     * Returns the total number of sprite @em sets.
     */
    int spriteCount();

    /**
     * Returns @c true iff a sprite exists for the specified @a spriteId and @a frame;
     *
     * @param spriteId  Unique identifier of the sprite set.
     * @param frame     Frame number from the set to lookup.
     */
    bool hasSprite(spritenum_t spriteId, int frame);

    /**
     * Returns a pointer to the identified Sprite.
     *
     * @see hasSprite()
     */
    inline Sprite *spritePtr(spritenum_t spriteId, int frame) {
        return hasSprite(spriteId, frame)? spriteSet(spriteId).at(frame) : 0;
    }

    /**
     * Lookup the sprite set for the specified @a spriteId.
     *
     * @param spriteId  Unique identifier of the sprite set.
     * @return  The identified SpriteSet.
     */
    SpriteSet const &spriteSet(spritenum_t spriteId);

#ifdef __CLIENT__
    /**
     * Precache resources from the set associated with the specified @a spriteId.
     *
     * @param spriteId      Unique identifier of the sprite set to cache.
     * @param materialSpec  Specification to use when caching materials.
     */
    void cacheSpriteSet(spritenum_t spriteId, de::MaterialVariantSpec const &materialSpec);
#endif

    /**
     * Provides access to the Textures collection.
     */
    de::Textures &textures();

    void initSystemTextures();

    /**
     * Convenient method of searching the texture collection for a texture with
     * the specified @a schemeName and @a resourceUri.
     *
     * @param schemeName  Unique name of the scheme in which to search.
     * @param resourceUri  Path to the (image) resource to find the texture for.
     *
     * @return  The found texture; otherwise @c 0.
     */
    de::Texture *texture(de::String schemeName, de::Uri const *resourceUri);

    de::Texture *defineTexture(de::String schemeName, de::Uri const &resourceUri,
                               de::Vector2i const &dimensions = de::Vector2i());

    patchid_t declarePatch(de::String encodedName);

#ifdef __CLIENT__
    /**
     * Provides access to the Fonts collection.
     */
    de::Fonts &fonts();

    /**
     * To be called during a definition database reset to clear all definitions
     * linked to by Font resources.
     */
    void clearFontDefinitionLinks();

    AbstractFont *createFontFromDef(ded_compositefont_t const &def);
    AbstractFont *createFontFromFile(de::Uri const &uri, de::String filePath);

#endif

    /**
     * Returns the total number of animation/precache groups.
     */
    int animGroupCount();

    /**
     * Destroys all the animation groups.
     */
    void clearAllAnimGroups();

    /**
     * Returns the AnimGroup associated with @a uniqueId (1-based); otherwise @c 0.
     */
    de::AnimGroup *animGroup(int uniqueId);

    /**
     * Construct a new animation group.
     *
     * @param flags  @ref animationGroupFlags
     */
    de::AnimGroup &newAnimGroup(int flags);

    /**
     * Returns the total number of color palettes.
     */
    int colorPaletteCount() const;

    /**
     * Destroys all the color palettes.
     */
    void clearAllColorPalettes();

    /**
     * Returns the ColorPalette associated with unique @a id.
     */
    ColorPalette &colorPalette(colorpaletteid_t id) const;

    /**
     * Returns the symbolic name of the specified color @a palette. A zero-length
     * string is returned if no name is associated.
     */
    de::String colorPaletteName(ColorPalette &palette) const;

    /**
     * Returns @c true iff a ColorPalette with the specified @a name is present.
     */
    bool hasColorPalette(de::String name) const;

    /**
     * Returns the ColorPalette associated with @a name.
     *
     * @see hasColorPalette()
     */
    ColorPalette &colorPalette(de::String name) const;

    /**
     * @param newPalette  Color palette to add. Ownership of the palette is given
     *                    to the resource system.
     * @param name        Symbolic name of the color palette.
     */
    void addColorPalette(ColorPalette &newPalette, de::String const &name = de::String());

    /**
     * Returns the unique identifier of the current default color palette.
     */
    colorpaletteid_t defaultColorPalette() const;

    /**
     * Change the default color palette.
     *
     * @param newDefaultPalette  The color palette to make default.
     */
    void setDefaultColorPalette(ColorPalette *newDefaultPalette);

public: /// @todo Should be private:
    void initCompositeTextures();
    void initFlatTextures();
    void initSpriteTextures();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_RESOURCESYSTEM_H
