# MidnightSun — Handoff Document

## Estado del Proyecto
**Fecha**: 2026-04-16
**Proyecto**: MidnightSun — Linux-only, headless-first game streaming server
**Fork de**: Sunshine (LizardByte)
**Repo**: https://github.com/KanonZVE/MidnightSun
**Branch**: `main`

---

## 1. Resumen Ejecutivo

MidnightSun es un fork de Sunshine enfocado en:
- **Headless Operation**: Streaming sin monitor físico
- **Auto-Resolution**: La resolución del stream coincide con lo que Moonlight pide
- **AMD GPU Optimized**: VAAPI encoding para AMD
- **Bazzite OS**: Target principal

### Problema que resuelve
Sunshine en Linux tiene bugs:
- Pierde certificados al cerrar/abrir sesiones
- No funciona con monitor apagado
- La resolución del display no coincide con la del cliente

---

## 2. Estado Actual

### ✅ Completado

| Component | Estado | Archivos |
|-----------|--------|----------|
| HeadlessDisplay class | ✅ | `src/platform/linux/headless.h`, `headless.cpp` |
| VKMS integration | ✅ | `src/platform/linux/kmsgrab.cpp` |
| Software fallback | ✅ | `src/platform/linux/headless.cpp` |
| Gamescope compatibility | ✅ | `src/platform/linux/headless.cpp` |
| Certificate persistence | ✅ | `src/nvhttp.cpp` |
| Error handling & logging | ✅ | `src/platform/linux/headless.cpp`, `misc.cpp` |
| KMS capture integration | ✅ | `src/platform/linux/kmsgrab.cpp` |
| Source registration | ✅ | `src/platform/linux/misc.cpp` |
| Documentation | ✅ | `README.md`, `docs/` |
| SDD artifacts | ✅ | `openspec/` |
| AppImage build infrastructure | ✅ | `.github/workflows/build-appimage.yml` |

### ⏳ Pendiente

| Task | Razón | Qué hacer |
|------|-------|-----------|
| **1.2 Config Extension** | Código pendiente | Agregar `headless_t` config struct |
| **4.1 Manual Testing** | Requiere hardware | Probar en Bazzite con AMD GPU |
| **4.2 Unit Tests** | Requiere hardware | Tests limitados posibles |

---

## 3. Continuar con Task 1.2: Config Extension

### Qué hacer

Agregar configuración de headless en `src/config.h` y `src/config.cpp`:

#### 3.1 En `src/config.h`

Buscar la sección de video config y agregar:

```cpp
struct headless_t {
    bool enabled = false;          // false = auto-detect, true = force on, never = force off
    int fallback_width = 1920;     // fallback resolution if VKMS fails
    int fallback_height = 1080;
    int fallback_framerate = 60;
    bool prefer_vkms = true;       // prefer VKMS over software fallback
};
```

Y en la struct de video:
```cpp
struct video_t {
    // ... existing fields ...
    headless_t headless;
};
```

#### 3.2 En `src/config.cpp`

Agregar parsing de la config headless:

```cpp
void parse_headless(const std::unordered_map<std::string, std::string> &vars) {
    bool_to_string(vars, "headless_enabled", video.headless.enabled);
    int_between_f(vars, "headless_fallback_width", video.headless.fallback_width, {640, 7680});
    int_between_f(vars, "headless_fallback_height", video.headless.fallback_height, {480, 4320});
    int_between_f(vars, "headless_fallback_framerate", video.headless.fallback_framerate, {30, 240});
    bool_to_string(vars, "headless_prefer_vkms", video.headless.prefer_vkms);
}
```

#### 3.3 En `src/platform/linux/misc.cpp`

Actualizar `verify_headless()` para usar la config:

```cpp
bool verify_headless() {
    // Check if explicitly disabled
    if (!config::video.headless.enabled) {
        // Still auto-detect if not explicitly disabled
        return platf::headless::is_headless();
    }
    
    // If explicitly enabled, always use headless
    return true;
}
```

#### 3.4 En `src/platform/linux/headless.cpp`

Usar la config para fallback:

```cpp
std::unique_ptr<HeadlessDisplay> HeadlessDisplay::create(
    int width, int height, int refresh_rate) {
    
    // Use config fallback if dimensions not specified
    if (width <= 0 || height <= 0) {
        width = config::video.headless.fallback_width;
        height = config::video.headless.fallback_height;
        refresh_rate = config::video.headless.fallback_framerate;
    }
    
    // ... rest of implementation ...
}
```

---

## 4. Continuar con Task 4.2: Unit Tests

### Tests que SÍ se pueden hacer sin hardware

#### 4.1 Test de detección headless

```cpp
// tests/test_headless_detection.cpp
#include <gtest/gtest.h>
#include "src/platform/linux/headless.h"

TEST(HeadlessDetectionTest, IsGamescopeActive) {
    // Test with environment variables
    setenv("GAMESCOPE_WIDTH", "1920", 1);
    EXPECT_TRUE(platf::headless::is_gamescope_active());
    unsetenv("GAMESCOPE_WIDTH");
    
    EXPECT_FALSE(platf::headless::is_gamescope_active());
}

TEST(HeadlessDetectionTest, IsVkmsAvailable) {
    // This will return false on systems without VKMS
    // But the function itself should not crash
    bool available = platf::headless::HeadlessDisplay::is_vkms_available();
    // Just verify it doesn't crash
    SUCCEED();
}
```

#### 4.2 Test de config parsing

```cpp
// tests/test_headless_config.cpp
#include <gtest/gtest.h>
#include "src/config.h"

TEST(HeadlessConfigTest, DefaultValues) {
    config::video_t video;
    
    EXPECT_FALSE(video.headless.enabled);
    EXPECT_EQ(video.headless.fallback_width, 1920);
    EXPECT_EQ(video.headless.fallback_height, 1080);
    EXPECT_EQ(video.headless.fallback_framerate, 60);
    EXPECT_TRUE(video.headless.prefer_vkms);
}
```

### Tests que NO se pueden hacer sin hardware

- ❌ VKMS creation (requires `/dev/dri/`)
- ❌ KMS capture (requires DRM)
- ❌ Actual streaming (requires full pipeline)

---

## 5. Continuar con Task 4.1: Manual Testing

### Testing Matrix

| Test Case | Monitor | Resolution | Expected Result |
|-----------|---------|-----------|-----------------|
| Headless 1080p | Disconnected | 1920x1080 | VKMS @ 1920x1080 |
| Headless 1440p | Disconnected | 2560x1440 | VKMS @ 2560x1440 |
| Headless 4K | Disconnected | 3840x2160 | VKMS @ 3840x2160 |
| With Monitor | Connected | 1920x1080 | Physical display used |
| VKMS Unavailable | Disconnected | Any | Software fallback |
| Gamescope Active | Steam Deck | Any | Gamescope output used |

### Testing Steps

1. **Build AppImage** (ya configurado):
   ```bash
   ./scripts/build-appimage.sh
   ```

2. **Run AppImage**:
   ```bash
   chmod +x artifacts/MidnightSun-x86_64.AppImage
   ./artifacts/MidnightSun-x86_64.AppImage --help
   ```

3. **Test headless**:
   - Desconectar monitor
   - Ejecutar MidnightSun
   - Conectar desde Moonlight
   - Verificar resolución

4. **Check logs**:
   ```bash
   # Enable debug logging
   export BOOST_LOG_SEVERITY=debug
   ./artifacts/MidnightSun-x86_64.AppImage
   ```

### Logs a verificar

```
[info] No physical displays detected. Activating headless mode.
[info] Creating headless virtual display at 1920x1080@60Hz
[info] Found VKMS card for headless capture: /dev/dri/card1
[info] Headless mode activated via VKMS at 1920x1080@60Hz
```

---

## 6. AppImage Build

### Estado actual

El workflow de GitHub Actions está configurado para:
- Ubuntu 22.04
- CMake con flags: `SUNSHINE_BUILD_APPIMAGE=ON`, `BUILD_DOCS=OFF`, `BUILD_TESTS=OFF`
- Submódulos clonados manualmente
- wayland-protocols del sistema
- wlr-protocols con nuestro XML

### Ejecutar build

1. Ve a: https://github.com/KanonZVE/MidnightSun/actions
2. Click en "Build AppImage"
3. Click en "Run workflow"
4. Selecciona `main`
5. Click en "Run workflow"

### Descargar AppImage

Cuando termine:
1. Ve al run del workflow
2. Scroll hasta "Artifacts"
3. Click en "MidnightSun-AppImage"

---

## 7. Archivos Clave

### Código fuente modificado

| Archivo | Cambio |
|---------|--------|
| `src/platform/linux/headless.h` | HeadlessDisplay class |
| `src/platform/linux/headless.cpp` | VKMS + software implementation |
| `src/platform/linux/kmsgrab.cpp` | KMS integration |
| `src/platform/linux/misc.cpp` | Source registration |
| `src/nvhttp.cpp` | Certificate persistence |
| `cmake/compile_definitions/linux.cmake` | Appindicator logic fix |
| `CMakeLists.txt` | Project renaming |

### Archivos de configuración

| Archivo | Propósito |
|---------|-----------|
| `.github/workflows/build-appimage.yml` | CI/CD workflow |
| `packaging/linux/AppImage/AppRun` | AppImage entry point |
| `packaging/linux/AppImage/com.midnightsun.app.desktop` | Desktop file |

### Documentación

| Archivo | Contenido |
|---------|-----------|
| `README.md` | Project overview |
| `docs/ARCHITECTURE.md` | Sunshine architecture analysis |
| `docs/ROADMAP.md` | Implementation roadmap |
| `docs/PRD.md` | Product requirements |
| `openspec/changes/headless-auto-resolution/` | SDD artifacts |

---

## 8. Problemas Conocidos

### 1. XML Validation Warning

```
WARNING: XML failed validation against built-in DTD
wlr-screencopy-unstable-v1.xml:2: element protocol: validity error
```

**Status**: WARNING only, no error. El protocolo funciona correctamente.

### 2. CUDA Optional

```
-- Looking for a CUDA compiler - NOTFOUND
```

**Status**: Normal. CUDA es opcional para AMD GPUs.

### 3. Appindicator Not Found

```
-- Checking for module 'ayatana-appindicator3-0.1' - not found
-- Checking for module 'appindicator3-0.1' - not found
```

**Status**: Normal. libnotify se usa como fallback. Ya está fixeado.

---

## 9. SDD Status

### Artefactos creados

| Artefacto | Ubicación |
|-----------|-----------|
| Proposal | `openspec/changes/headless-auto-resolution/PROPOSAL.md` |
| Specs | `openspec/changes/headless-auto-resolution/SPEC.md` |
| Design | `openspec/changes/headless-auto-resolution/DESIGN.md` |
| Tasks | `openspec/changes/headless-auto-resolution/TASKS.md` |

### Verificación

- **Status**: PASS WITH WARNINGS
- **Tasks completadas**: 11/14
- **Requisitos**: 7/7 con evidencia estructural
- **Pendiente**: 1.2 Config, 4.1 Testing, 4.2 Tests

---

## 10. Próximos Pasos

### Inmediatos (en tu casa)

1. **Ejecutar AppImage build** en GitHub Actions
2. **Descargar AppImage** y probar
3. **Implementar Task 1.2** (Config Extension)
4. **Agregar Task 4.2** (Unit Tests básicos)

### Futuro

1. **Task 4.1**: Manual testing en Bazzite
2. **HDR Support**: Implementar soporte HDR
3. **AV1 Enhancement**: Mejorar encoding AV1
4. **Multi-client**: Soporte para múltiples clientes
5. **Modern UI**: Nueva interfaz web

---

## 11. Referencias

- [Sunshine Repository](https://github.com/LizardByte/Sunshine)
- [Moonlight Project](https://moonlight-stream.org)
- [VKMS Documentation](https://docs.kernel.org/gpu/vkms.html)
- [VAAPI Documentation](https://github.com/intel/libva)

---

## 12. Contacto

- **Repo**: https://github.com/KanonZVE/MidnightSun
- **Issues**: https://github.com/KanonZVE/MidnightSun/issues

---

*Documento generado el 2026-04-16 para continuar el desarrollo de MidnightSun.*
