// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !SLIMPELLER

#include "flutter/shell/common/snapshot_controller_skia.h"

#include "display_list/image/dl_image.h"
#include "flutter/flow/surface.h"
#include "flutter/fml/trace_event.h"
#include "flutter/shell/common/snapshot_controller.h"
#include "fml/build_config.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"

#if FML_OS_ANDROID
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#endif

#if defined(FML_OS_IOS) || defined(FML_OS_MACOSX)
#include "third_party/skia/include/gpu/ganesh/mtl/GrMtlBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/mtl/GrMtlTypes.h"
#endif

namespace flutter {

namespace {
sk_sp<SkImage> DrawSnapshot(
    const sk_sp<SkSurface>& surface,
    const std::function<void(SkCanvas*)>& draw_callback) {
  if (surface == nullptr || surface->getCanvas() == nullptr) {
    return nullptr;
  }

  draw_callback(surface->getCanvas());
  auto dContext = GrAsDirectContext(surface->recordingContext());
  if (dContext) {
    dContext->flushAndSubmit();
  }

  sk_sp<SkImage> device_snapshot;
  {
    TRACE_EVENT0("flutter", "MakeDeviceSnapshot");
    device_snapshot = surface->makeImageSnapshot();
  }

  if (device_snapshot == nullptr) {
    return nullptr;
  }

  {
    TRACE_EVENT0("flutter", "DeviceHostTransfer");
    if (auto raster_image = device_snapshot->makeRasterImage(nullptr)) {
      return raster_image;
    }
  }

  return nullptr;
}
}  // namespace

void SnapshotControllerSkia::MakeRasterSnapshot(
    sk_sp<DisplayList> display_list,
    SkISize picture_size,
    std::function<void(const sk_sp<DlImage>&)> callback) {
  callback(MakeRasterSnapshotSync(display_list, picture_size));
}

sk_sp<DlImage> SnapshotControllerSkia::MakeFromTexture(int64_t raw_texture,
                                                       SkISize size) {
  GrBackendTexture texture;
  SkColorType color_type;
#if defined(FML_OS_ANDROID)
  // GL_RGBA8 0x8058_(OES)
  uint32_t format = 0x8058;
  // GL_TEXTURE_EXTERNAL_OES
  uint32_t target = 0x8D65;
  const GrGLTextureInfo texture_info{target, static_cast<GrGLuint>(raw_texture),
                                     format};
  texture = GrBackendTextures::MakeGL(size.width(), size.height(),
                                      skgpu::Mipmapped::kNo, texture_info);
  color_type = SkColorType::kRGBA_8888_SkColorType;
#elif defined(FML_OS_IOS) || defined(FML_OS_MACOSX)
  GrMtlTextureInfo texture_info;
  texture_info.fTexture =
      sk_cfp<const void*>(reinterpret_cast<const void*>(raw_texture));
  texture = GrBackendTextures::MakeMtl(size.width(), size.height(),
                                       skgpu::Mipmapped::kNo, texture_info);
  color_type = SkColorType::kBGRA_8888_SkColorType;
#else
  texture = GrBackendTexture();
  color_type = kRGBA_8888_SkColorType;
#endif
  static const auto color_space = SkColorSpace::MakeSRGB();
  const auto& delegate = GetDelegate();
  if (delegate.GetSurface() && delegate.GetSurface()->GetContext()) {
    const auto image = SkImages::BorrowTextureFrom(
        delegate.GetSurface()->GetContext(), texture, kTopLeft_GrSurfaceOrigin,
        color_type, kPremul_SkAlphaType, color_space);
    return DlImage::Make(image);
  }
  return nullptr;
}

std::unique_ptr<Surface> SnapshotControllerSkia::MakeOffscreenSurface(
    int64_t raw_texture,
    const SkISize& size) {
  GrBackendTexture texture;
  SkColorType color_type;
#if defined(FML_OS_ANDROID)
  GrGLTextureInfo texture_info;
  texture_info.fTarget = 0x0DE1;  // GR_GL_TEXTURE2D_2D;
  texture_info.fID = raw_texture;
  texture_info.fFormat = 0x8058;  // GR_GL_RGBA8;
  texture = GrBackendTextures::MakeGL(size.width(), size.height(),
                                      skgpu::Mipmapped::kNo, texture_info);
  color_type = SkColorType::kRGBA_8888_SkColorType;
#elif defined(FML_OS_IOS) || defined(FML_OS_MACOSX)
  GrMtlTextureInfo texture_info;
  texture_info.fTexture =
      sk_cfp<const void*>(reinterpret_cast<const void*>(raw_texture));
  texture = GrBackendTextures::MakeMtl(size.width(), size.height(),
                                       skgpu::Mipmapped::kNo, texture_info);
  color_type = SkColorType::kBGRA_8888_SkColorType;
#else
  texture = GrBackendTexture();
  color_type = kRGBA_8888_SkColorType;
#endif
  static const auto color_space = SkColorSpace::MakeSRGB();
  auto context = GetDelegate().GetSurface()->GetContext();
  auto surface = SkSurfaces::WrapBackendTexture(
      context, texture, kBottomLeft_GrSurfaceOrigin, 1, color_type, color_space,
      nullptr, nullptr, nullptr);
  return std::make_unique<OffscreenSkiaSurface>(surface, context);
}

sk_sp<DlImage> SnapshotControllerSkia::DoMakeRasterSnapshot(
    SkISize size,
    std::function<void(SkCanvas*)> draw_callback) {
  TRACE_EVENT0("flutter", __FUNCTION__);
  sk_sp<SkImage> result;
  SkImageInfo image_info = SkImageInfo::MakeN32Premul(
      size.width(), size.height(), SkColorSpace::MakeSRGB());

  std::unique_ptr<Surface> pbuffer_surface;
  Surface* snapshot_surface = nullptr;
  auto& delegate = GetDelegate();
  if (delegate.GetSurface() && delegate.GetSurface()->GetContext()) {
    snapshot_surface = delegate.GetSurface().get();
  } else if (delegate.GetSnapshotSurfaceProducer()) {
    pbuffer_surface =
        delegate.GetSnapshotSurfaceProducer()->CreateSnapshotSurface();
    if (pbuffer_surface && pbuffer_surface->GetContext()) {
      snapshot_surface = pbuffer_surface.get();
    }
  }

  if (!snapshot_surface) {
    // Raster surface is fine if there is no on screen surface. This might
    // happen in case of software rendering.
    sk_sp<SkSurface> sk_surface = SkSurfaces::Raster(image_info);
    result = DrawSnapshot(sk_surface, draw_callback);
  } else {
    delegate.GetIsGpuDisabledSyncSwitch()->Execute(
        fml::SyncSwitch::Handlers()
            .SetIfTrue([&] {
              sk_sp<SkSurface> surface = SkSurfaces::Raster(image_info);
              result = DrawSnapshot(surface, draw_callback);
            })
            .SetIfFalse([&] {
              FML_DCHECK(snapshot_surface);
              auto context_switch =
                  snapshot_surface->MakeRenderContextCurrent();
              if (!context_switch->GetResult()) {
                return;
              }

              GrRecordingContext* context = snapshot_surface->GetContext();
              auto max_size = context->maxRenderTargetSize();
              double scale_factor = std::min(
                  1.0, static_cast<double>(max_size) /
                           static_cast<double>(std::max(image_info.width(),
                                                        image_info.height())));

              // Scale down the render target size to the max supported by the
              // GPU if necessary. Exceeding the max would otherwise cause a
              // null result.
              if (scale_factor < 1.0) {
                image_info = image_info.makeWH(
                    static_cast<double>(image_info.width()) * scale_factor,
                    static_cast<double>(image_info.height()) * scale_factor);
              }

              // When there is an on screen surface, we need a render target
              // SkSurface because we want to access texture backed images.
              sk_sp<SkSurface> sk_surface =
                  SkSurfaces::RenderTarget(context,               // context
                                           skgpu::Budgeted::kNo,  // budgeted
                                           image_info             // image info
                  );
              if (!sk_surface) {
                FML_LOG(ERROR)
                    << "DoMakeRasterSnapshot can not create GPU render target";
                return;
              }

              sk_surface->getCanvas()->scale(scale_factor, scale_factor);
              result = DrawSnapshot(sk_surface, draw_callback);
            }));
  }

  // It is up to the caller to create a DlImageGPU version of this image
  // if the result will interact with the UI thread.
  return DlImage::Make(result);
}

sk_sp<DlImage> SnapshotControllerSkia::MakeRasterSnapshotSync(
    sk_sp<DisplayList> display_list,
    SkISize size) {
  return DoMakeRasterSnapshot(size, [display_list](SkCanvas* canvas) {
    DlSkCanvasAdapter(canvas).DrawDisplayList(display_list);
  });
}

sk_sp<SkImage> SnapshotControllerSkia::ConvertToRasterImage(
    sk_sp<SkImage> image) {
  // If the rasterizer does not have a surface with a GrContext, then it will
  // be unable to render a cross-context SkImage.  The caller will need to
  // create the raster image on the IO thread.
  if (GetDelegate().GetSurface() == nullptr ||
      GetDelegate().GetSurface()->GetContext() == nullptr) {
    return nullptr;
  }

  if (image == nullptr) {
    return nullptr;
  }

  SkISize image_size = image->dimensions();

  auto result = DoMakeRasterSnapshot(
      image_size, [image = std::move(image)](SkCanvas* canvas) {
        canvas->drawImage(image, 0, 0);
      });
  return result->skia_image();
}

void SnapshotControllerSkia::CacheRuntimeStage(
    const std::shared_ptr<impeller::RuntimeStage>& runtime_stage) {}

SnapshotControllerSkia::OffscreenSkiaSurface::OffscreenSkiaSurface(
    sk_sp<SkSurface> surface,
    GrDirectContext* context)
    : _surface(std::move(surface)), _context(context) {}

SnapshotControllerSkia::OffscreenSkiaSurface::~OffscreenSkiaSurface() = default;

bool SnapshotControllerSkia::OffscreenSkiaSurface::IsValid() {
  return _surface != nullptr;
}

std::unique_ptr<SurfaceFrame>
SnapshotControllerSkia::OffscreenSkiaSurface::AcquireFrame(
    const SkISize& size) {
  auto encode_callback = [](const SurfaceFrame& surface_frame,
                            DlCanvas* canvas) -> bool {
    canvas->Flush();
    return true;
  };
  auto submit_callback = [](const SurfaceFrame& surface_frame) -> bool {
    return true;
  };
  SurfaceFrame::FramebufferInfo framebuffer_info;
  framebuffer_info.supports_readback = true;
  return std::make_unique<SurfaceFrame>(_surface, framebuffer_info,
                                        encode_callback, submit_callback, size);
}

SkMatrix SnapshotControllerSkia::OffscreenSkiaSurface::GetRootTransformation()
    const {
  return {};
}

GrDirectContext* SnapshotControllerSkia::OffscreenSkiaSurface::GetContext() {
  return _context;
}
}  // namespace flutter

#endif  //  !SLIMPELLER
