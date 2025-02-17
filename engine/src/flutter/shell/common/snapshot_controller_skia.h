// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_COMMON_SNAPSHOT_CONTROLLER_SKIA_H_
#define FLUTTER_SHELL_COMMON_SNAPSHOT_CONTROLLER_SKIA_H_

#if !SLIMPELLER

#include "flutter/shell/common/snapshot_controller.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace flutter {

class SnapshotControllerSkia : public SnapshotController {
 public:
  explicit SnapshotControllerSkia(const SnapshotController::Delegate& delegate)
      : SnapshotController(delegate) {}

  void MakeRasterSnapshot(
      sk_sp<DisplayList> display_list,
      SkISize picture_size,
      std::function<void(const sk_sp<DlImage>&)> callback) override;

  sk_sp<DlImage> MakeRasterSnapshotSync(sk_sp<DisplayList> display_list,
                                        SkISize size) override;

  virtual sk_sp<SkImage> ConvertToRasterImage(sk_sp<SkImage> image) override;

  void CacheRuntimeStage(
      const std::shared_ptr<impeller::RuntimeStage>& runtime_stage) override;

  sk_sp<DlImage> MakeFromTexture(int64_t raw_texture, SkISize size) override;

  std::unique_ptr<Surface> MakeOffscreenSurface(int64_t raw_texture,
                                                const SkISize& size) override;

 private:
  sk_sp<DlImage> DoMakeRasterSnapshot(
      SkISize size,
      std::function<void(SkCanvas*)> draw_callback);

  FML_DISALLOW_COPY_AND_ASSIGN(SnapshotControllerSkia);

  class OffscreenSkiaSurface : public Surface {
   public:
    OffscreenSkiaSurface(sk_sp<SkSurface> surface, GrDirectContext* context);

    ~OffscreenSkiaSurface() override;

    bool IsValid() override;

    std::unique_ptr<SurfaceFrame> AcquireFrame(const SkISize& size) override;

    SkMatrix GetRootTransformation() const override;

    GrDirectContext* GetContext() override;

   private:
    sk_sp<SkSurface> _surface;
    GrDirectContext* _context;
  };
};

}  // namespace flutter

#endif  //  !SLIMPELLER

#endif  // FLUTTER_SHELL_COMMON_SNAPSHOT_CONTROLLER_SKIA_H_
