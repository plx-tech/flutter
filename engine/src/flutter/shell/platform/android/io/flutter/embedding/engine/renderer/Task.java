package io.flutter.embedding.engine.renderer;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

@Keep
public interface Task {
  @SuppressWarnings("unused")
  public void run(@NonNull boolean isGpuDisabled);
}
