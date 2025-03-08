#include "application.hpp"

#if DEBUG
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#endif

#include "log.hpp"
#include "utility.hpp"
#include "material.hpp"

MemoryArena GlobalArena;
MemoryArena TransientArena;

#if DEBUG
void ImGuiMemoryDiagnosticsWindow()
{
    ImGui::Begin("Memory diagnostics");
    ImGui::Text("Arena memory: %zu", MemoryArena::TotalAllocationSize);
    ImGui::Text("Num arenas: %zu", MemoryArena::NumActiveArenas);

    MemoryArena::MemoryInfo globalArenaInfo = GlobalArena.GetMemoryInfo();
    ImGui::Text("Global arena: %zu/%zu", globalArenaInfo.usedMemory, globalArenaInfo.allocatedMemory);

    MemoryArena::MemoryInfo transientArenaInfo = TransientArena.GetMemoryInfo();
    ImGui::Text("Transient arena: %zu/%zu", transientArenaInfo.usedMemory, transientArenaInfo.allocatedMemory);

    MemoryArena::MemoryInfo logArenaInfo = Log::GetArena()->GetMemoryInfo();
    ImGui::Text("Log arena: %zu/%zu", logArenaInfo.usedMemory, logArenaInfo.allocatedMemory);

    ImGui::End();
}
#endif

bool Application::Init(GameState startGameState)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return false;
    }

    m_Window = SDL_CreateWindow("", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
    if (m_Window == nullptr)
    {
        Log::Error("Failed to create window");
        return false;
    }

    GlobalArena.Init(1'048'576, MemoryArenaFlags_ClearToZero);
    TransientArena.Init(1'048'576, MemoryArenaFlags_ClearToZero);

    if (!m_Renderer.Init(m_Window))
    {
        Log::Error("Failed to initialize renderer");
        return false;
    }

    m_Scene.Init(&GlobalArena);
    m_Menu.Init(&GlobalArena);

    LoadScene(firstSceneFilepath);
    m_GameState = startGameState;

    return true;
}

bool Application::Loop(float deltaTime)
{
    constexpr float TARGET_DELTA_TIME = 1.0f / 60.0f;
    static float timeAccumulation = 0.0f;
    timeAccumulation += deltaTime;
    if (timeAccumulation < TARGET_DELTA_TIME)
    {
        return true;
    }
    timeAccumulation -= TARGET_DELTA_TIME;

    bool success = false;
    if (m_GameState == GameState::Editor)
    {
        success = LoopEditor(TARGET_DELTA_TIME);
    }
    else if ((int)m_GameState & (int)GameState::MainMenu)
    {
        success = LoopMainMenu(TARGET_DELTA_TIME);
    }
    else if (m_GameState == GameState::Game || m_GameState == GameState::FinishingLevel)
    {
        success = LoopGame(TARGET_DELTA_TIME);
    }
    else
    {
        Log::Error("Invalid game state: %", (int)m_GameState);
    }
    TransientArena.Clear();
    return success;
}

void Application::LoadScene(StringView sceneFilepath)
{
    m_Scene.Clear();
    m_Scene.Deserialize(ReadFile(sceneFilepath, &TransientArena));

    Entity* playerEntity = m_Scene.CreateEntity();
    playerEntity->material = MaterialManager::GetMaterial("player");
    playerEntity->transform.position = Math::float2(0.0f, 0.0f);
    playerEntity->transform.scale = Math::float2(0.1);
    playerEntity->shape = Shape::Ellipse;
    playerEntity->flags |= (uint16_t)EntityFlags::Player;
    m_Player = Player(playerEntity);
}

Entity* GetHoveredEntity(const Scene& scene, Math::float2 worldPosition)
{
    Entity* closestEntity = nullptr;
    auto HitEntity = [&closestEntity](Entity* entity)
    {
        if (closestEntity == nullptr || entity->zIndex > closestEntity->zIndex)
        {
            closestEntity = entity;
        }
    };

    for (Entity& entity : scene.entities)
    {
        Math::float2 rotatedWorldPosition = entity.transform.position + Math::RotateVector(worldPosition - entity.transform.position, -entity.transform.rotation);
        switch (entity.shape)
        {
            case Shape::Ellipse:
            {
                float distanceSquared = Math::DistanceSquared(
                    entity.transform.position / entity.transform.scale,
                    rotatedWorldPosition / entity.transform.scale
                );
                if (distanceSquared <= 1.0f)
                {
                    HitEntity(&entity);
                }
                break;
            }
            case Shape::Rectangle:
            {
                Math::float2 min = entity.transform.position - entity.transform.scale;
                Math::float2 max = entity.transform.position + entity.transform.scale;
                if (rotatedWorldPosition.x >= min.x && rotatedWorldPosition.y >= min.y &&
                    rotatedWorldPosition.x <= max.x && rotatedWorldPosition.y <= max.y)
                {
                    HitEntity(&entity);
                }
                break;
            }
            default:
                Log::Warn("Invalid shape: %", (int)entity.shape);
        }
    }
    return closestEntity;
}

#if DEBUG
void ImGuiFlag(const char* label, uint16_t& flags, uint16_t mask)
{
    uint32_t value = flags;
    ImGui::CheckboxFlags(label, &value, mask);
    flags = value;
}
#endif

bool Application::LoopEditor(float deltaTime)
{
    m_Renderer.NewFrame(deltaTime);

    m_Renderer.renderHiddenEntities = true;

    static float cameraSpeed = 0.05f;
    m_Camera.transform.position += cameraSpeed * m_Camera.transform.scale * m_Input.Joystick();

    Math::float2 mouseWorldPosition;
    {
        Math::float2 windowDimensions = { (float)m_Renderer.GetWidth(), (float)m_Renderer.GetHeight() };
        Math::float2 viewPosition = (2.0f * m_Input.GetMousePosition() / windowDimensions) - 1.0f;
        viewPosition.x *= windowDimensions.x / windowDimensions.y;
        viewPosition.y *= -1;
        mouseWorldPosition = m_Camera.transform.position + viewPosition * m_Camera.transform.scale;

        // Quantize the position to a multiple of 0.01f
        mouseWorldPosition = Math::Floor(mouseWorldPosition * 100.0f) / 100.0f;
    }

    static Entity* inspectedEntity = nullptr;
    static Entity* templateEntity = nullptr;
    if (m_Input.IsMousePressed())
    {
        inspectedEntity = GetHoveredEntity(m_Scene, mouseWorldPosition);
        if (inspectedEntity != nullptr)
        {
            templateEntity = inspectedEntity;
        }
    }

    if (m_Input.IsKeyPressed(SDL_SCANCODE_N))
    {
        inspectedEntity = m_Scene.CreateEntity();
        inspectedEntity->transform.position = mouseWorldPosition;
        inspectedEntity->transform.scale = 0.1f;

        if (templateEntity != nullptr)
        {
            inspectedEntity->flags = templateEntity->flags;
            inspectedEntity->zIndex = templateEntity->zIndex;
            inspectedEntity->transform.scale = templateEntity->transform.scale;
            inspectedEntity->transform.rotation = templateEntity->transform.rotation;
            inspectedEntity->material = templateEntity->material;
            inspectedEntity->shape = templateEntity->shape;
            inspectedEntity->gravityZone = templateEntity->gravityZone;
            if (m_Input.IsKeyDown(SDL_SCANCODE_LSHIFT))
            {
                inspectedEntity->transform.position = templateEntity->transform.position;
            }
        }
    }

    if (inspectedEntity != nullptr && m_Input.IsKeyPressed(SDL_SCANCODE_BACKSPACE))
    {
        m_Scene.DestroyEntity(inspectedEntity);
        inspectedEntity = nullptr;
        templateEntity = nullptr;
    }

#if DEBUG
    ImGuiMemoryDiagnosticsWindow();

    ImGui::Begin("Inspector");
    if (inspectedEntity != nullptr)
    {
        // ImGui::InputText("Name:", &inspectedEntity->name);

        ImGui::Text("ID: %i", inspectedEntity->id);

        char buffer[100] {};
        memcpy(buffer, inspectedEntity->name.data, Math::Min(sizeof(buffer) - 1, inspectedEntity->name.size));
        if (ImGui::InputText("Name", buffer, sizeof(buffer)))
        {
            // TODO: Make a proper String overload similar to imgui_stdlib
            inspectedEntity->name.Clear();
            inspectedEntity->name += buffer;
        }

        ImGuiFlag("Flag: Collider",     inspectedEntity->flags, (uint16_t)EntityFlags::Collider);
        ImGuiFlag("Flag: Gravity Zone", inspectedEntity->flags, (uint16_t)EntityFlags::GravityZone);
        ImGuiFlag("Flag: Text",         inspectedEntity->flags, (uint16_t)EntityFlags::Text);
        ImGuiFlag("Flag: Hidden",       inspectedEntity->flags, (uint16_t)EntityFlags::Hidden);
        ImGuiFlag("Flag: Light",        inspectedEntity->flags, (uint16_t)EntityFlags::Light);
        ImGuiFlag("Flag: Lava",         inspectedEntity->flags, (uint16_t)EntityFlags::Lava);
        ImGuiFlag("Flag: Death Zone",   inspectedEntity->flags, (uint16_t)EntityFlags::DeathZone);
        ImGuiFlag("Flag: Checkpoint",   inspectedEntity->flags, (uint16_t)EntityFlags::Checkpoint);
        ImGuiFlag("Flag: Exit",         inspectedEntity->flags, (uint16_t)EntityFlags::Exit);

        int zIndex = inspectedEntity->zIndex;
        ImGui::InputInt("Z-index", &zIndex);
        inspectedEntity->zIndex = zIndex;

        ImGui::DragFloat2("Position", (float*)&inspectedEntity->transform.position, 0.01);
        ImGui::DragFloat2("Scale", (float*)&inspectedEntity->transform.scale, 0.01);

        int rotation = std::roundf(inspectedEntity->transform.rotation * Math::RAD_TO_DEG);
        ImGui::DragInt("Rotation", &rotation);
        inspectedEntity->transform.rotation = (float)rotation * Math::DEG_TO_RAD;

        // TODO: Material properties
        // ImGui::ColorEdit3("Color", (float*)&inspectedEntity->color);

        String materialName = String::Copy(inspectedEntity->material->name, &TransientArena);
        materialName.NullTerminate();
        if (ImGui::BeginCombo("material", materialName.data))
        {
            Array<String> materialNames = MaterialManager::GetMaterialNames(&TransientArena);
            for (String name : materialNames)
            {
                name.NullTerminate();
                if (ImGui::Selectable(name.data, name == materialName))
                {
                    inspectedEntity->material = MaterialManager::GetMaterial(name);
                }
            }
            ImGui::EndCombo();
        }

        static constexpr std::array<const char*, 2> shapes {
            "Rectangle", "Ellipse"
        };
        if (ImGui::BeginCombo("Shape", shapes[(int)inspectedEntity->shape]))
        {
            for (int i = 0; i < (int)shapes.size(); i++)
            {
                if (ImGui::Selectable(shapes[i], i == (int)inspectedEntity->shape))
                {
                    inspectedEntity->shape = (Shape)i;
                }
            }
            ImGui::EndCombo();
        }

        if (inspectedEntity->flags & (uint16_t)EntityFlags::GravityZone)
        {
            if (inspectedEntity->shape == Shape::Ellipse)
            {
                int minAngle = (int)std::floorf(inspectedEntity->gravityZone.minAngle * Math::RAD_TO_DEG);
                int maxAngle = (int)std::floorf(inspectedEntity->gravityZone.maxAngle * Math::RAD_TO_DEG);
                ImGui::Text("Radial Gravity Zone (degrees):");
                ImGui::DragInt("Min angle", &minAngle, 1, -180, 180);
                ImGui::DragInt("Max angle", &maxAngle, 1, -180, 180);
                inspectedEntity->gravityZone.minAngle = minAngle * Math::DEG_TO_RAD;
                inspectedEntity->gravityZone.maxAngle = maxAngle * Math::DEG_TO_RAD;
            }
        }
    }
    ImGui::End();

    ImGui::Begin("Editor");
    ImGui::ColorEdit3("Background color", (float*)&m_Scene.properties.backgroundColor);
    ImGui::CheckboxFlags("Lock camera Y",        (int*)&m_Scene.properties.flags, (int)Scene::Properties::Flags::LockCameraY);
    ImGui::CheckboxFlags("Lock camera rotation", (int*)&m_Scene.properties.flags, (int)Scene::Properties::Flags::LockCameraRotation);
    ImGui::DragFloat2("Camera position", (float*)&m_Camera.transform.position, 0.01f);
    ImGui::DragFloat("Camera speed", &cameraSpeed, 0.01f);
    ImGui::DragFloat("Camera scale", &m_Camera.transform.scale.x, 0.01f);
    m_Camera.transform.scale.y = m_Camera.transform.scale.x;

    static char sceneFilepath[100] { 0 };
    ImGui::InputText("Scene path:", sceneFilepath, sizeof(sceneFilepath));
    if (ImGui::Button("Load"))
    {
        LoadScene(sceneFilepath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        WriteFile(sceneFilepath, m_Scene.Serialize(&TransientArena));
    }

    ImGui::End();
#endif

    m_Input.EndFrame();
    m_Scene.EndFrame();

    m_Renderer.Resize();
    m_Camera.aspect = (float)m_Renderer.GetWidth() / (float)m_Renderer.GetHeight();
    return m_Renderer.Render(m_Scene, m_Camera);
}

bool Application::LoopMainMenu(float deltaTime)
{
    m_Renderer.NewFrame(deltaTime);

    Math::float2 mousePosition = m_Input.GetMousePosition();
    mousePosition /= Math::float2(m_Renderer.GetWidth(), m_Renderer.GetHeight());
    mousePosition = mousePosition * 2.0f - 1.0f;
    mousePosition *= { (float)m_Renderer.GetWidth() / m_Renderer.GetHeight(), -1.0f };

    m_Menu.Begin(mousePosition, m_Input.IsMousePressed());

    m_Menu.SetTextMaterial("main_menu_text");
    m_Menu.SetButtonMaterial("main_menu_button");

    const Math::float2 buttonSize = { 0.5f, 0.07f };
    const Math::float2 buttonPadding = { 0.0f, 0.05f };

    bool shouldExit = false;
    switch (m_GameState)
    {
        case GameState::MainMenu_MainMenu:
            m_Menu.Text("Untitled Platformer", { 0.0f, 0.5f }, 0.1f);
            if (m_Menu.Button("Start Game", { 0.0f, 0.05f }, buttonSize, buttonPadding))
            {
                m_GameState = GameState::Game;
            }
            if (m_Menu.Button("Controls", { 0.0f, -0.25f }, buttonSize, buttonPadding))
            {
                m_GameState = GameState::MainMenu_Controls;
            }
            if (m_Menu.Button("Exit Game", { 0.0f, -0.55f }, buttonSize, buttonPadding))
            {
                shouldExit = true;
            }
            break;
        case GameState::MainMenu_Controls:
            m_Menu.Text("Controls", { 0.0f, 0.75f }, 0.1f);
            m_Menu.Text("Click a on key binding to change it.", { 0.0f, 0.57f }, 0.035f);
            m_Menu.Text("Enter ESCAPE to reset a binding.",     { 0.0f, 0.49f }, 0.035f);
            for (int i = 0; i < (int)m_Input.controls.size(); i++)
            {
                float y = (float)(2 - i) * 0.17f;
                m_Menu.Text(KeyNames[i], { -0.8f, y }, 0.06f);
                for (int j = 0; j < (int)m_Input.controls[i].size(); j++)
                {
                    bool selected = i == m_ActiveControlRebind[0] && j == m_ActiveControlRebind[1];
                    if (selected && m_Input.GetFirstKeyPressed() != SDL_SCANCODE_UNKNOWN)
                    {
                        if (m_Input.GetFirstKeyPressed() == SDL_SCANCODE_ESCAPE)
                        {
                            m_Input.controls[i][j] = SDL_SCANCODE_UNKNOWN;
                        }
                        else {
                            m_Input.controls[i][j] = m_Input.GetFirstKeyPressed();
                        }
                        selected = false;
                        m_ActiveControlRebind = { -1, -1 };
                    }

                    SDL_Scancode scancode = m_Input.controls[i][j];
                    SDL_Keycode keycode = SDL_GetKeyFromScancode(scancode, 0, true);
                    const char* keyName = nullptr;
                    if (selected)
                    {
                        keyName = "[Press any key]";
                    }
                    else
                    {
                        switch (keycode)
                        {
                            case SDLK_MINUS:        keyName = "Minus";         break;
                            case SDLK_EQUALS:       keyName = "Equals";        break;
                            case SDLK_LEFTBRACKET:  keyName = "Left Bracket";  break;
                            case SDLK_RIGHTBRACKET: keyName = "Right Bracket"; break;
                            case SDLK_BACKSLASH:    keyName = "Backslash";     break;
                            case SDLK_HASH:         keyName = "Hashtag";       break;
                            case SDLK_SEMICOLON:    keyName = "Semicolon";     break;
                            case SDLK_APOSTROPHE:   keyName = "Apostrophe";    break;
                            case SDLK_GRAVE:        keyName = "Backtick";      break;
                            case SDLK_COMMA:        keyName = "Comma";         break;
                            case SDLK_PERIOD:       keyName = "Period";        break;
                            case SDLK_SLASH:        keyName = "Forward Slash"; break;
                            default: keyName = SDL_GetKeyName(keycode);
                        }
                    }

                    float x = (float)j * 0.5f - 0.3f;
                    if (m_Menu.Button(keyName, { x, y }, { 0.23f, 0.03f }, { 0.0f, 0.03f }))
                    {
                        m_ActiveControlRebind = { i, j };
                    }
                }
            }
            if (m_Menu.Button("Back", { 0.0f, -0.8f }, buttonSize, buttonPadding))
            {
                m_GameState = GameState::MainMenu_MainMenu;
                m_ActiveControlRebind = { -1, -1 };
            }
            break;
        default:
            Log::Error("Invalid game state: %", (int)m_GameState);
            break;
    }

    m_Input.EndFrame();
    m_Scene.EndFrame();

#if DEBUG
    ImGuiMemoryDiagnosticsWindow();
#endif

    m_Renderer.Resize();

    Camera camera { .aspect = (float)m_Renderer.GetWidth() / m_Renderer.GetHeight() };
    return m_Renderer.Render(m_Menu.scene, camera) && !shouldExit;
}

bool Application::LoopGame(float deltaTime)
{
    static double frameTime = 0.0;
    uint64_t frameStart = SDL_GetTicksNS();

    m_Renderer.NewFrame(deltaTime);

    if (m_GameState == GameState::Game)
    {
        bool finishedLevel = false;
        m_Player.Update(m_Scene, m_Camera.transform.rotation, m_Renderer.GetTime(), m_Input, &finishedLevel);
        if (finishedLevel)
        {
            m_GameState = GameState::FinishingLevel;
        }

        Math::float2 down = m_Scene.properties.flags & (uint32_t)Scene::Properties::Flags::LockCameraRotation ?
            Math::float2(0, -1) : m_Player.gravityDirection;

        Math::float2 right = m_Scene.properties.flags & (uint32_t)Scene::Properties::Flags::LockCameraRotation ?
            Math::float2(1, 0) : Math::float2{ -m_Player.gravityDirection.y, m_Player.gravityDirection.x };

        Math::float2 cameraOffset = { 0.0f, 0.3f };
        cameraOffset.x = (std::exp(Math::Dot(m_Player.velocity, right)) - 1.0f) * 20.0f;
        m_Camera.FollowPlayer(m_Player.GetTransform().position, cameraOffset, down, deltaTime);
        if (m_Scene.properties.flags & (uint32_t)Scene::Properties::Flags::LockCameraY)
        {
            m_Camera.transform.position.y = 0.0f;
        }
        if (m_Scene.properties.flags & (uint32_t)Scene::Properties::Flags::LockCameraRotation)
        {
            m_Camera.transform.rotation = 0.0f;
        }
    }
    else
    {
        assert(m_GameState == GameState::FinishingLevel);
        if (m_LevelFinishCounter++ > s_LevelFinishFrames)
        {
            m_LevelFinishCounter = 0;
            m_GameState = GameState::MainMenu_MainMenu;
        }
    }

    m_Input.EndFrame();
    m_Scene.EndFrame();

#if DEBUG
    ImGuiMemoryDiagnosticsWindow();

    ImGui::Text("CPU: %fms", frameTime);
    ImGui::Text("Delta time: %fms", deltaTime * 1000.0f);
    ImGui::Text("Camera position: (%f, %f)", m_Camera.transform.position.x, m_Camera.transform.position.y);
    ImGui::Text("Player position: (%f, %f)", m_Player.GetTransform().position.x, m_Player.GetTransform().position.y);
    ImGui::Text("Player velocity: (%f, %f)", m_Player.velocity.x, m_Player.velocity.y);
    ImGui::Text("Player speed: %f", Math::Length(m_Player.velocity));
    ImGui::DragFloat("Player acceleration", &m_Player.acceleration, 0.001f);
    ImGui::DragFloat("Player jump acceleration", &m_Player.jumpAcceleration, 0.001f);
    ImGui::DragFloat("Player gravity acceleration", &m_Player.gravityAcceleration, 0.001f);
    ImGui::DragFloat("Player drag", &m_Player.drag, 0.001f);
    ImGui::Text("Gravity direction: (%f, %f)", m_Player.gravityDirection.x, m_Player.gravityDirection.y);

    if (ImGui::TreeNode("Debug Textures"))
    {
        m_Renderer.ImGuiDebugTextures();
        ImGui::TreePop();
    }
#endif

    m_Renderer.Resize();
    m_Camera.aspect = (float)m_Renderer.GetWidth() / (float)m_Renderer.GetHeight();

    frameTime = (double)(SDL_GetTicksNS() - frameStart) / 1'000'000.0;
    return m_Renderer.Render(m_Scene, m_Camera);
}

void Application::Exit()
{
    SDL_DestroyWindow(m_Window);
}

void Application::OnEvent(const SDL_Event& event)
{
#if DEBUG
    ImGui_ImplSDL3_ProcessEvent(&event);
#endif
    switch (event.type)
    {
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_MOUSE_ADDED:
        case SDL_EVENT_MOUSE_REMOVED:
#if DEBUG
            if (!ImGui::GetIO().WantCaptureMouse)
#endif
            {
                m_Input.OnEvent(event);
            }
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_EDITING:
        case SDL_EVENT_TEXT_INPUT:
        case SDL_EVENT_KEYMAP_CHANGED:
#if DEBUG
            if (!ImGui::GetIO().WantCaptureKeyboard)
#endif
            {
                m_Input.OnEvent(event);
            }
            break;
        default:
            m_Input.OnEvent(event);
    }
}
