//
//  TextureCache.h
//  interface
//
//  Created by Andrzej Kapolka on 8/6/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#ifndef __interface__TextureCache__
#define __interface__TextureCache__

#include <QImage>
#include <QMap>

#include <ResourceCache.h>

#include "InterfaceConfig.h"

class QOpenGLFramebufferObject;

class NetworkTexture;

/// Stores cached textures, including render-to-texture targets.
class TextureCache : public ResourceCache {
    Q_OBJECT
    
public:
    
    TextureCache();
    virtual ~TextureCache();
    
    /// Returns the ID of the permutation/normal texture used for Perlin noise shader programs.  This texture
    /// has two lines: the first, a set of random numbers in [0, 255] to be used as permutation offsets, and
    /// the second, a set of random unit vectors to be used as noise gradients.
    GLuint getPermutationNormalTextureID();

    /// Returns the ID of an opaque white texture (useful for a default).
    GLuint getWhiteTextureID();

    /// Returns the ID of a pale blue texture (useful for a normal map).
    GLuint getBlueTextureID();

    /// Loads a texture from the specified URL.
    QSharedPointer<NetworkTexture> getTexture(const QUrl& url, bool normalMap = false, bool dilatable = false);

    /// Returns a pointer to the primary framebuffer object.  This render target includes a depth component, and is
    /// used for scene rendering.
    QOpenGLFramebufferObject* getPrimaryFramebufferObject();
    
    /// Returns the ID of the primary framebuffer object's depth texture.  This contains the Z buffer used in rendering.
    GLuint getPrimaryDepthTextureID();
    
    /// Returns a pointer to the secondary framebuffer object, used as an additional render target when performing full
    /// screen effects.
    QOpenGLFramebufferObject* getSecondaryFramebufferObject();
    
    /// Returns a pointer to the tertiary framebuffer object, used as an additional render target when performing full
    /// screen effects.
    QOpenGLFramebufferObject* getTertiaryFramebufferObject();
    
    /// Returns a pointer to the framebuffer object used to render shadow maps.
    QOpenGLFramebufferObject* getShadowFramebufferObject();
    
    /// Returns the ID of the shadow framebuffer object's depth texture.
    GLuint getShadowDepthTextureID();
    
    virtual bool eventFilter(QObject* watched, QEvent* event);

protected:

    virtual QSharedPointer<Resource> createResource(const QUrl& url,
        const QSharedPointer<Resource>& fallback, bool delayLoad, const void* extra);
        
private:
    
    friend class DilatableNetworkTexture;
    
    QOpenGLFramebufferObject* createFramebufferObject();
    
    GLuint _permutationNormalTextureID;
    GLuint _whiteTextureID;
    GLuint _blueTextureID;
    
    QHash<QUrl, QWeakPointer<NetworkTexture> > _dilatableNetworkTextures;
    
    GLuint _primaryDepthTextureID;
    QOpenGLFramebufferObject* _primaryFramebufferObject;
    QOpenGLFramebufferObject* _secondaryFramebufferObject;
    QOpenGLFramebufferObject* _tertiaryFramebufferObject;
    
    QOpenGLFramebufferObject* _shadowFramebufferObject;
    GLuint _shadowDepthTextureID;
};

/// A simple object wrapper for an OpenGL texture.
class Texture {
public:
    
    Texture();
    ~Texture();

    GLuint getID() const { return _id; }

private:
    
    GLuint _id;
};

/// A texture loaded from the network.
class NetworkTexture : public Resource, public Texture {
    Q_OBJECT

public:
    
    NetworkTexture(const QUrl& url, bool normalMap);

    /// Checks whether it "looks like" this texture is translucent
    /// (majority of pixels neither fully opaque or fully transparent).
    bool isTranslucent() const { return _translucent; }

protected:

    virtual void downloadFinished(QNetworkReply* reply);
    virtual void imageLoaded(const QImage& image);      

    Q_INVOKABLE void setImage(const QImage& image, bool translucent);

private:

    bool _translucent;
};

/// Caches derived, dilated textures.
class DilatableNetworkTexture : public NetworkTexture {
    Q_OBJECT
    
public:
    
    DilatableNetworkTexture(const QUrl& url);
    
    /// Returns a pointer to a texture with the requested amount of dilation.
    QSharedPointer<Texture> getDilatedTexture(float dilation);
    
protected:

    virtual void imageLoaded(const QImage& image);
    virtual void reinsert();
    
private:
    
    QImage _image;
    int _innerRadius;
    int _outerRadius;
    
    QMap<float, QWeakPointer<Texture> > _dilatedTextures;    
};

#endif /* defined(__interface__TextureCache__) */
