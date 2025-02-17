#include "flutter/lib/ui/painting/render_surface.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/lib/ui/ui_dart_state.h"
#include "fml/logging.h"
#include "third_party/tonic/dart_args.h"
#include "third_party/tonic/dart_persistent_value.h"

namespace flutter {

IMPLEMENT_WRAPPERTYPEINFO(ui, RenderSurface);

void RenderSurface::Create(Dart_Handle dart_handle, int64_t raw_texture) {
  auto render_surface = fml::MakeRefCounted<RenderSurface>(raw_texture);
  render_surface->AssociateWithDartWrapper(dart_handle);
}

RenderSurface::RenderSurface(int64_t raw_texture)
    : _surface{nullptr}, _raw_texture{raw_texture}, _size(DlISize()) {}

RenderSurface::~RenderSurface() = default;

void RenderSurface::setup(int32_t width, int32_t height, Dart_Handle callback) {
  const auto dart_state = UIDartState::Current();
  const auto ui_task_runner = dart_state->GetTaskRunners().GetUITaskRunner();
  const auto raster_task_runner =
      dart_state->GetTaskRunners().GetRasterTaskRunner();
  const auto snapshot_delegate = dart_state->GetSnapshotDelegate();
  auto persistent_callback =
      std::make_unique<tonic::DartPersistentValue>(dart_state, callback);

  const auto ui_task = fml::MakeCopyable(
      [persistent_callback = std::move(persistent_callback)]() mutable {
        auto dart_state = persistent_callback->dart_state().lock();
        if (!dart_state) {
          return;
        }
        tonic::DartState::Scope scope(dart_state);
        tonic::DartInvoke(persistent_callback->Get(), {});
        persistent_callback.reset();
      });

  const auto raster_task = fml::MakeCopyable(
      [ui_task = std::move(ui_task), ui_task_runner = std::move(ui_task_runner),
       snapshot_delegate = std::move(snapshot_delegate), render_surface = this,
       width, height]() {
        render_surface->_surface = snapshot_delegate->MakeOffscreenSurface(
            render_surface->_raw_texture, SkISize::Make(width, height));
        render_surface->_size = DlISize(width, height);
        fml::TaskRunner::RunNowOrPostTask(std::move(ui_task_runner),
                                          std::move(ui_task));
      });

  fml::TaskRunner::RunNowOrPostTask(std::move(raster_task_runner),
                                    std::move(raster_task));
}

void RenderSurface::dispose(Dart_Handle callback) {
  const auto dart_state = UIDartState::Current();
  const auto ui_task_runner = dart_state->GetTaskRunners().GetUITaskRunner();
  const auto raster_task_runner =
      dart_state->GetTaskRunners().GetRasterTaskRunner();
  const auto snapshot_delegate = dart_state->GetSnapshotDelegate();
  auto persistent_callback =
      std::make_unique<tonic::DartPersistentValue>(dart_state, callback);

  const auto ui_task = fml::MakeCopyable(
      [persistent_callback = std::move(persistent_callback)]() mutable {
        auto dart_state = persistent_callback->dart_state().lock();
        if (!dart_state) {
          return;
        }
        tonic::DartState::Scope scope(dart_state);
        tonic::DartInvoke(persistent_callback->Get(), {});
        persistent_callback.reset();
      });

  const auto raster_task = fml::MakeCopyable(
      [ui_task = std::move(ui_task), ui_task_runner = std::move(ui_task_runner),
       snapshot_delegate = std::move(snapshot_delegate),
       render_surface = this]() {
        render_surface->_surface.reset();
        fml::TaskRunner::RunNowOrPostTask(std::move(ui_task_runner),
                                          std::move(ui_task));
      });

  fml::TaskRunner::RunNowOrPostTask(std::move(raster_task_runner),
                                    std::move(raster_task));
}
}  // namespace flutter
