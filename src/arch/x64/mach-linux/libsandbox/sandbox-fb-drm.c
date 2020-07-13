#include <x.h>
#include <sandbox.h>

struct sandbox_fb_drm_context_t {
	int fd;
	uint32_t width;
	uint32_t height;
	uint32_t crtc_id;
	uint32_t conn_id;
	drmModeConnector * conn;
};

struct fb_drm_buf_t {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t size;
	uint32_t handle;
	void * map;
	uint32_t fb;
};

void * sandbox_fb_drm_open(const char * dev)
{
	struct sandbox_fb_drm_context_t * ctx;
	drmModeRes * res;
	drmModeConnector * conn;
	uint64_t dumb;

	ctx = malloc(sizeof(struct sandbox_fb_drm_context_t));
	if(!ctx)
		return NULL;

	ctx->fd = open(dev, O_RDWR | O_CLOEXEC);
	if(ctx->fd < 0)
	{
		free(ctx);
		return NULL;
	}

	if(drmGetCap(ctx->fd, DRM_CAP_DUMB_BUFFER, &dumb) < 0 || !dumb)
	{
		close(ctx->fd);
		free(ctx);
		return NULL;
	}

	res = drmModeGetResources(ctx->fd);
	if(!res)
	{
		close(ctx->fd);
		free(ctx);
		return NULL;
	}

	ctx->crtc_id = res->crtcs[0];
	ctx->conn_id = res->connectors[0];
	conn = drmModeGetConnector(ctx->fd, ctx->conn_id);
	if(!conn)
	{
		close(ctx->fd);
		free(ctx);
		return NULL;
	}
	ctx->conn = conn;
	ctx->width = conn->modes[0].hdisplay;
	ctx->height = conn->modes[0].vdisplay;

	return ctx;
}

void sandbox_fb_drm_close(void * context)
{
	struct sandbox_fb_drm_context_t * ctx = (struct sandbox_fb_drm_context_t *)context;

	if(ctx)
	{
		close(ctx->fd);
		free(ctx);
	}
}

int sandbox_fb_drm_get_width(void * context)
{
	struct sandbox_fb_drm_context_t * ctx = (struct sandbox_fb_drm_context_t *)context;
	return ctx->width;
}

int sandbox_fb_drm_get_height(void * context)
{
	struct sandbox_fb_drm_context_t * ctx = (struct sandbox_fb_drm_context_t *)context;
	return ctx->height;
}

int sandbox_fb_drm_get_pwidth(void * context)
{
	struct sandbox_fb_drm_context_t * ctx = (struct sandbox_fb_drm_context_t *)context;
	if(ctx)
		return 256;
	return 0;
}

int sandbox_fb_drm_get_pheight(void * context)
{
	struct sandbox_fb_drm_context_t * ctx = (struct sandbox_fb_drm_context_t *)context;
	if(ctx)
		return 135;
	return 0;
}

int sandbox_fb_drm_surface_create(void * context, struct sandbox_fb_surface_t * surface)
{
	struct sandbox_fb_drm_context_t * ctx = (struct sandbox_fb_drm_context_t *)context;
	struct fb_drm_buf_t * drmbuf;
	struct drm_mode_create_dumb creq;
	struct drm_mode_destroy_dumb dreq;
	struct drm_mode_map_dumb mreq;

	drmbuf = malloc(sizeof(struct fb_drm_buf_t));
	if(!drmbuf)
		return 0;

	memset(&creq, 0, sizeof(creq));
	creq.width = ctx->width;
	creq.height = ctx->height;
	creq.bpp = 32;
	if(drmIoctl(ctx->fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) != 0)
	{
		free(drmbuf);
		return 0;
	}

	drmbuf->width = creq.width;
	drmbuf->height = creq.height;
	drmbuf->stride = creq.pitch;
	drmbuf->size = creq.size;
	drmbuf->handle = creq.handle;
	memset(&dreq, 0, sizeof(dreq));
	dreq.handle = drmbuf->handle;

	if(drmModeAddFB(ctx->fd, drmbuf->width, drmbuf->height, 24, 32, drmbuf->stride, drmbuf->handle, &drmbuf->fb) != 0)
	{
		drmIoctl(ctx->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
		free(drmbuf);
		return 0;
	}

	memset(&mreq, 0, sizeof(mreq));
	mreq.handle = drmbuf->handle;
	if(drmIoctl(ctx->fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) != 0)
	{
		drmIoctl(ctx->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
		free(drmbuf);
		return 0;
	}

	drmbuf->map = mmap(0, drmbuf->size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, mreq.offset);
	if(drmbuf->map == MAP_FAILED)
	{
		drmModeRmFB(ctx->fd, drmbuf->fb);
		drmIoctl(ctx->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
		free(drmbuf);
		return 0;
	}
	memset(drmbuf->map, 0, drmbuf->size);

	surface->width = drmbuf->width;
	surface->height = drmbuf->height;
	surface->stride = drmbuf->stride;
	surface->pixlen = drmbuf->size;
	surface->pixels = drmbuf->map;
	surface->priv = drmbuf;

	return 1;
}

int sandbox_fb_drm_surface_destroy(void * context, struct sandbox_fb_surface_t * surface)
{
	struct sandbox_fb_drm_context_t * ctx = (struct sandbox_fb_drm_context_t *)context;
	struct fb_drm_buf_t * drmbuf = (struct fb_drm_buf_t *)surface->priv;
	struct drm_mode_destroy_dumb dreq;

	if(drmbuf)
	{
		munmap(drmbuf->map, drmbuf->size);
		drmModeRmFB(ctx->fd, drmbuf->fb);
		memset(&dreq, 0, sizeof(dreq));
		dreq.handle = drmbuf->handle;
		drmIoctl(ctx->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
		free(drmbuf);
	}
	return 1;
}

int sandbox_fb_drm_surface_present(void * context, struct sandbox_fb_surface_t * surface, struct sandbox_fb_region_list_t * rl)
{
	struct sandbox_fb_drm_context_t * ctx = (struct sandbox_fb_drm_context_t *)context;
	struct fb_drm_buf_t * drmbuf = (struct fb_drm_buf_t *)surface->priv;

	drmModeSetCrtc(ctx->fd, ctx->crtc_id, drmbuf->fb, 0, 0, &ctx->conn_id, 1, &ctx->conn->modes[0]);
	return 1;
}

void sandbox_fb_drm_set_backlight(void * context, int brightness)
{
}

int sandbox_fb_drm_get_backlight(void * context)
{
	return 0;
}
