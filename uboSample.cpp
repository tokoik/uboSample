///
/// ユニフォームバッファオブジェクトサンプル
///
/// @file
/// @author Kohe Tokoi
/// @date July 17, 2025
///
#include "GgApp.h"

// プロジェクト名
#if !defined(PROJECT_NAME)
#  define PROJECT_NAME "uboSample"
#endif

/// 出力画像の横幅
const GLsizei width{ 960 };

/// 出力画像の高さ
const GLsizei height{ 540 };

/// 3 要素のベクトルデータ型
using vec3 = std::array<GLfloat, 3>;

/// 4 要素のベクトルデータ型
using vec4 = std::array<GLfloat, 4>;

///
/// 視点
///
/// @note
/// スクリーンの高さを 1 として視点とスクリーンの距離
/// |rigin - position| を焦点距離に用いる
///
struct Camera
{
  /// スクリーンの原点
  alignas(16) vec3 origin;

  /// スクリーンの右方向
  alignas(16) vec3 right;

  /// スクリーンの上方向
  alignas(16) vec3 up;

  /// 視点の位置
  alignas(16) vec3 position;
};

///
/// 光源
///
struct Light
{
  // 環境光成分
  alignas(16) vec4 ambient;

  // 拡散反射光成分
  alignas(16) vec4 diffuse;

  // 鏡面反射光成分
  alignas(16) vec4 specular;

  // 位置
  alignas(16) vec4 position;
};

///
/// 材質
///
struct Material
{
  /// 環境光に対する反射係数
  alignas(16) vec4 ambient;

  /// 拡散反射反射係数
  alignas(16) vec4 diffuse;

  /// 鏡面反射反射係数
  alignas(16) vec4 specular;

  /// 輝き係数
  alignas(4) float shininess;
};

///
/// 球
///
struct Sphere
{
  /// 中心位置
  alignas(16) vec3 center;

  /// 半径
  float radius;

  /// 材質のインデックス
  int materialIndex;
};

///
/// 3 要素ベクトルの内積
///
/// @param a 3 要素ベクトル
/// @param b 3 要素ベクトル
/// @return a と b の内積
///
static auto dot(const vec3& a, const vec3& b)
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

///
/// 3 要素ベクトルの外積
///
/// @param a 3 要素ベクトル
/// @param b 3 要素ベクトル
/// @return a と b の外積
///
/// @note a と b の順序に注意
/// 
static auto cross(const vec3& a, const vec3& b)  
{  
  const auto x{ a[1] * b[2] - a[2] * b[1] };  
  const auto y{ a[2] * b[0] - a[0] * b[2] };  
  const auto z{ a[0] * b[1] - a[1] * b[0] };  
  return vec3{ x, y, z };  
}

///
/// 3 要素ベクトルの長さ
/// 
/// @param a 3 要素ベクトル
/// @return a の長さ
///
static auto length(const vec3& a)
{
  return std::sqrt(dot(a, a));
}

///
/// 3 要素ベクトルの正規化
///
/// @param a 3 要素ベクトル
/// @return a の正規化されたベクトル
///
/// @note ゼロベクトルはそのまま返す
///
static auto normalize(const vec3& a)
{
  const auto len{ length(a) };
  if (fabs(len) < std::numeric_limits<float>::epsilon()) return a;
  return vec3{ a[0] / len, a[1] / len, a[2] / len };
}

///
/// 視野の設定
///
/// @param camera 設定対象のカメラ
/// @param position 視点の位置
/// @param target 目標点の位置
/// @param up 上方向のベクトル
/// @param fovy 画角
///
/// @note
/// 視点の位置と目標点の位置を結ぶベクトルを視線ベクトルとし、
/// スクリーンの原点を視線ベクトルの焦点距離だけ前方に設定する。
/// スクリーンの右方向のベクトルは視線ベクトルと上方向のベクトルの外積を正規化する。
/// スクリーンの上方向のベクトルはスクリーンの右方向のベクトルと視線ベクトルの外積を正規化する。
/// 
static void setCamera(Camera& camera, const vec3& position, const vec3& target, const vec3& up, float fovy)
{
  // 視線ベクトル
  const vec3 forward
  {
    normalize(
      {
        target[0] - position[0],
        target[1] - position[1],
        target[2] - position[2]
      }
    )
  };

  // スクリーンの右方向のベクトル
  camera.right = normalize(cross(forward, up));

  // スクリーンの上方向のベクトル
  camera.up = cross(camera.right, forward);

  // 焦点距離
  const auto focal{ 1.0f / tanf(fovy * 0.5f * 3.1415927f / 180.0f) };

  // スクリーンの原点
  camera.origin =
  {
    position[0] + forward[0] * focal,
    position[1] + forward[1] * focal,
    position[2] + forward[2] * focal
  };

  // 視点の位置
  camera.position = position;
}

///
/// アプリケーション本体
///
/// @param argc コマンドライン引数の数
/// @param argv コマンドライン引数の文字列
/// @return 終了コード
/// 
int GgApp::main(int argc, const char* const* argv)
{
  // ウィンドウを作成する
  Window window{ PROJECT_NAME, width, height };

  // ウィンドウが作成できなかったらエラーを表示して終了する
  if (window.get() == nullptr) throw std::runtime_error("ウィンドウの作成に失敗しました");

  // コンピュートシェーダ
  const auto shader{ ggLoadComputeShader("raycast.comp") };

  // シェーダの読み込みに失敗したらエラーを表示して終了する
  if (shader == 0) throw std::runtime_error("シェーダの読み込みに失敗しました");

  // uniform 変数の場所
  const auto lightCountLoc{ glGetUniformLocation(shader, "lightCount") };
  const auto sphereCountLoc{ glGetUniformLocation(shader, "sphereCount") };
  const auto imageLoc{ glGetUniformLocation(shader, "image") };

  // 視点の位置
  vec3 position{ 0.0f, 0.0f, 2.0f };

  // 目標点の位置
  vec3 target{ 0.0f, 0.0f, 0.0f };

  // 上方向のベクトル
  vec3 up{ 0.0f, 1.0f, 0.0f };

  // 画角
  float fovy{ 60.0f };

  // 視点データ
  Camera camera;

  // 視点の設定を初期化する
  setCamera(camera, position, target, up, fovy);

  // 視点のユニフォームバッファオブジェクト
  GLuint cameraUbo;
  glGenBuffers(1, &cameraUbo);
  glBindBuffer(GL_UNIFORM_BUFFER, cameraUbo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof camera, &camera, GL_STATIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  // 光源のデータ
  std::array<Light, 2> light
  {
    0.2f, 0.2f, 0.2f, 1.0f,
    1.0f, 1.0f, 1.0f, 0.0f,
    1.0f, 1.0f, 1.0f, 0.0f,
    3.0f, 4.0f, 5.0f, 1.0f,

    0.1f, 0.1f, 0.0f, 1.0f,
    0.5f, 0.5f, 0.0f, 0.0f,
    0.5f, 0.5f, 0.0f, 0.0f,
    -5.0f, 1.0f, 3.0f, 1.0f,
  };

  // 光源のユニフォームバッファオブジェクト
  GLuint lightUbo;
  glGenBuffers(1, &lightUbo);
  glBindBuffer(GL_UNIFORM_BUFFER, lightUbo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof light, &light, GL_STATIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  // 光源のデータの数
  const auto lightCount{ static_cast<GLint>(light.size()) };

  // 設定対象の光源
  auto targetLight{ 0 };

  // 材質のデータ
  std::array<Material, 2> material
  {
    0.6f, 0.1f, 0.1f, 1.0f,
    0.6f, 0.1f, 0.1f, 0.0f,
    0.3f, 0.3f, 0.3f, 0.0f,
    100.0f,

    0.1f, 0.1f, 0.6f, 1.0f,
    0.1f, 0.1f, 0.6f, 0.0f,
    0.3f, 0.3f, 0.3f, 0.0f,
    100.0f
  };

  // 材質のユニフォームバッファオブジェクト
  GLuint materialUbo;
  glGenBuffers(1, &materialUbo);
  glBindBuffer(GL_UNIFORM_BUFFER, materialUbo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof material, &material, GL_STATIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  // 材質のデータの数
  const auto materialCount{ static_cast<GLint>(material.size()) };

  // 設定対象の材質
  auto targetMaterial{ 0 };

  // 球のデータ
  std::array<Sphere, 2> sphere
  {
    1.0f, 0.0f, -2.0f,
    1.0f,
    0,

    -1.0f, 0.0f, -1.0f,
    1.0f,
    1
  };

  // 球のシェーダストレージバッファオブジェクト
  GLuint sphereSsbo;
  glGenBuffers(1, &sphereSsbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereSsbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof sphere, &sphere, GL_STATIC_DRAW);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  // 球のデータの数
  const auto sphereCount{ static_cast<GLint>(sphere.size()) };

  // フレームバッファオブジェクトのカラーバッファに使うテクスチャ
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  // レンダリング先のフレームバッファオブジェクト
  GLuint framebuffer;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Image Unit の番号を設定する
  constexpr GLuint ImageUnit{ 0 };

  // メニューの表示
  bool showMenu{ false };

  // ウィンドウが開いている間繰り返す
  while (window)
  {
    // タブキーをタイプしたらメニューを表示する
    showMenu = showMenu || glfwGetKey(window.get(), GLFW_KEY_TAB);

    // メニューを表示するなら
    if (showMenu)
    {
      // メニューの表示領域を設定する
      ImGui::SetNextWindowPos(ImVec2(2, 2), ImGuiCond_Once);
      ImGui::SetNextWindowSize(ImVec2(308, 512), ImGuiCond_Once);

      // メニューの開始
      ImGui::Begin(u8"コントロールパネル", &showMenu);

      //
      // 視点の設定
      //
      ImGui::SeparatorText(u8"視点");
      auto cameraChanged{ false };
      cameraChanged |= ImGui::DragFloat3(u8"視点位置", position.data(), 0.01f);
      cameraChanged |= ImGui::DragFloat3(u8"目標点位置", target.data(), 0.01f);
      cameraChanged |= ImGui::DragFloat3(u8"上方向ベクトル", up.data(), 0.01f);
      cameraChanged |= ImGui::DragFloat(u8"画角", &fovy, 1.0f, 1.0f, 180.0f);

      // 視点が変更されたら
      if (cameraChanged)
      {
        // 視点の設定を更新する
        setCamera(camera, position, target, up, fovy);

        // 視点のユニフォームバッファオブジェクトを更新する
        glBindBuffer(GL_UNIFORM_BUFFER, cameraUbo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof camera, &camera);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
      }

      //
      // 光源の設定
      //
      ImGui::SeparatorText(u8"光源");
      ImGui::DragInt(u8"光源番号", &targetLight, 1.0f, 0, lightCount - 1);
      auto lightChanged{ false };
      lightChanged |= ImGui::DragFloat3(u8"光源位置", light[targetLight].position.data(), 0.01f);
      lightChanged |= ImGui::ColorEdit3(u8"環境光成分", light[targetLight].ambient.data());
      lightChanged |= ImGui::ColorEdit3(u8"拡散反射光成分", light[targetLight].diffuse.data());
      lightChanged |= ImGui::ColorEdit3(u8"鏡面反射光成分", light[targetLight].specular.data());

      // 光源が変更されたら
      if (lightChanged)
      {
        // 光源のユニフォームバッファオブジェクトを更新する
        glBindBuffer(GL_UNIFORM_BUFFER, lightUbo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof light, &light);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
      }

      //
      // 材質の設定
      //
      ImGui::SeparatorText(u8"材質");
      ImGui::DragInt(u8"材質番号", &targetMaterial, 1.0f, 0, materialCount - 1);
      auto materialChanged{ false };
      materialChanged |= ImGui::ColorEdit3(u8"環境光反射係数", material[targetMaterial].ambient.data());
      materialChanged |= ImGui::ColorEdit3(u8"拡散反射係数", material[targetMaterial].diffuse.data());
      materialChanged |= ImGui::ColorEdit3(u8"鏡面反射係数", material[targetMaterial].specular.data());
      materialChanged |= ImGui::DragFloat(u8"輝き係数", &material[targetMaterial].shininess, 1.0f, 1.0f, 1000.0f);

      // 材質が変更されたら
      if (materialChanged)
      {
        // 材質のユニフォームバッファオブジェクトを更新する
        glBindBuffer(GL_UNIFORM_BUFFER, materialUbo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof material, &material);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
      }

      // メニューの終了
      ImGui::End();
    }

    // 球のデータのシェーダストレージバッファオブジェクトを 0 番の結合ポイントに結合する
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sphereSsbo);

    // 視点のユニフォームバッファオブジェクトを 1 番の結合ポイントに結合する
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, cameraUbo);

    // 光源のユニフォームバッファオブジェクトを 2 番の結合ポイントに結合する
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, lightUbo);

    // 材質のユニフォームバッファオブジェクトを 3 番の結合ポイントに結合する
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, materialUbo);

    // コンピュートシェーダを指定する
    glUseProgram(shader);

    // 光源のデータの数と球のデータの数を指定する
    glUniform1i(lightCountLoc, lightCount);
    glUniform1i(sphereCountLoc, sphereCount);

    // 書き込み先のイメージを指定する
    glUniform1i(imageLoc, ImageUnit);

    // texture を image unit に結合する
    glBindImageTexture(ImageUnit, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    // ワークグループを画素ごとに起動する
    glDispatchCompute(width, height, 1);

    // シェーダの実行が完了するまで待機する
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // イメージの結合を解除する
    glBindImageTexture(ImageUnit, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    // シェーダの使用を終了する
    glUseProgram(0);

    // シェーダストレージバッファオブジェクトの結合を解除する
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

    // ユニフォームバッファオブジェクトの結合を解除する
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, 0);

    // シーンを描画する
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glBlitFramebuffer(0, 0, width, height, 0, 0, window.getWidth(), window.getHeight(), GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    // カラーバッファを入れ替えてイベントを取り出す
    window.swapBuffers();
  }

  // フレームバッファオブジェクトを削除する
  glDeleteFramebuffers(1, &framebuffer);

  // カラーバッファのテクスチャを削除する
  glDeleteTextures(1, &texture);

  // シェーダストレージバッファオブジェクトを削除する
  glDeleteBuffers(1, &sphereSsbo);

  // ユニフォームバッファオブジェクトを削除する
  glDeleteBuffers(1, &materialUbo);
  glDeleteBuffers(1, &lightUbo);
  glDeleteBuffers(1, &cameraUbo);

  // シェーダを削除する
  glDeleteProgram(shader);

  return 0;
}
