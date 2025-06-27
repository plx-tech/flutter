// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/image.h"

#include <algorithm>
#include <limits>
#include "tonic/logging/dart_invoke.h"

#if IMPELLER_SUPPORTS_RENDERING
#include "flutter/lib/ui/painting/image_encoding_impeller.h"
#endif
#include "flutter/lib/ui/painting/display_list_deferred_image_gpu_skia.h"
#include "flutter/lib/ui/painting/image_encoding.h"
#include "flutter/lib/ui/ui_dart_state.h"
#include "third_party/tonic/converter/dart_converter.h"
#include "third_party/tonic/dart_args.h"
#include "third_party/tonic/dart_binding_macros.h"
#include "third_party/tonic/dart_library_natives.h"
#if IMPELLER_SUPPORTS_RENDERING
#include "flutter/lib/ui/painting/display_list_deferred_image_gpu_impeller.h"
#endif  // IMPELLER_SUPPORTS_RENDERING

namespace flutter {

typedef CanvasImage Image;

// Since _Image is a private class, we can't use IMPLEMENT_WRAPPERTYPEINFO
static const tonic::DartWrapperInfo kDartWrapperInfoUIImage("ui", "_Image");
const tonic::DartWrapperInfo& Image::dart_wrapper_info_ =
    kDartWrapperInfoUIImage;

CanvasImage::CanvasImage() = default;

CanvasImage::~CanvasImage() = default;

Dart_Handle CanvasImage::CreateOuterWrapping() {
  Dart_Handle ui_lib = Dart_LookupLibrary(tonic::ToDart("dart:ui"));
  return tonic::DartInvokeField(ui_lib, "_wrapImage", {ToDart(this)});
}

Dart_Handle CanvasImage::toByteData(int format, Dart_Handle callback) {
  return EncodeImage(this, format, callback);
}

void CanvasImage::dispose() {
  image_.reset();
  ClearDartWrapper();
}

int CanvasImage::colorSpace() {
  if (image_->skia_image()) {
    return ColorSpace::kSRGB;
  } else if (image_->impeller_texture()) {
#if IMPELLER_SUPPORTS_RENDERING
    return ImageEncodingImpeller::GetColorSpace(image_->impeller_texture());
#endif  // IMPELLER_SUPPORTS_RENDERING
  }
  return ColorSpace::kSRGB;
}

static sk_sp<DlImage> CreateDeferredImageFromTexture(
    bool impeller,
    int64_t raw_texture,
    const SkISize& size,
    fml::TaskRunnerAffineWeakPtr<SnapshotDelegate> snapshot_delegate,
    fml::RefPtr<fml::TaskRunner> raster_task_runner,
    fml::RefPtr<SkiaUnrefQueue> unref_queue) {
#if SLIMPELLER
  return DlDeferredImageGPUImpeller::MakeFromTexture(
      raw_texture, size, std::move(snapshot_delegate),
      std::move(raster_task_runner));
#else
#if IMPELLER_SUPPORTS_RENDERING
  if (impeller) {
    return DlDeferredImageGPUImpeller::MakeFromTexture(
        raw_texture, size, std::move(snapshot_delegate),
        std::move(raster_task_runner));
  }
#endif  // IMPELLER_SUPPORTS_RENDERING
  return DlDeferredImageGPUSkia::MakeFromTexture(
      raw_texture, size, std::move(snapshot_delegate), raster_task_runner,
      std::move(unref_queue));
#endif  // SLIMPELLER
}

fml::RefPtr<flutter::CanvasImage> flutter::CanvasImage::CreateFromTextureID(
    int64_t texture_id,
    int32_t width,
    int32_t height) {
  auto* dart_state = UIDartState::Current();
  if (!dart_state) {
    return nullptr;
  }
  auto unref_queue = dart_state->GetSkiaUnrefQueue();
  auto snapshot_delegate = dart_state->GetSnapshotDelegate();
  auto raster_task_runner = dart_state->GetTaskRunners().GetRasterTaskRunner();

  auto image = CanvasImage::Create();
  auto dl_image = CreateDeferredImageFromTexture(
      dart_state->IsImpellerEnabled(), texture_id, SkISize::Make(width, height),
      std::move(snapshot_delegate), std::move(raster_task_runner),
      std::move(unref_queue));
  image->set_image(dl_image);
  return image;
}

fml::RefPtr<flutter::CanvasImage>
flutter::CanvasImage::CreateFromTexturePointer(int64_t texture_pointer,
                                               int32_t width,
                                               int32_t height) {
  auto* dart_state = UIDartState::Current();
  if (!dart_state) {
    return nullptr;
  }
  auto unref_queue = dart_state->GetSkiaUnrefQueue();
  auto snapshot_delegate = dart_state->GetSnapshotDelegate();
  auto raster_task_runner = dart_state->GetTaskRunners().GetRasterTaskRunner();

  auto image = CanvasImage::Create();
  auto dl_image = CreateDeferredImageFromTexture(
      dart_state->IsImpellerEnabled(), texture_pointer,
      SkISize::Make(width, height), std::move(snapshot_delegate),
      std::move(raster_task_runner), std::move(unref_queue));
  if (!dl_image) {
    return nullptr;
  }
  image->set_image(dl_image);
  return image;
}
}  // namespace flutter
