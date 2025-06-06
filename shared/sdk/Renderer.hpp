#pragma once

#include <cstdint>
#include <tuple>
#include <optional>

#include "ReClass.hpp"
#include "RENativeArray.hpp"
#include "renderer/RenderResource.hpp"
#include "renderer/PipelineState.hpp"
#include "intrusive_ptr.hpp"
#include "ManagedObject.hpp"

class REType;
struct ID3D12Resource;

namespace sdk {
namespace renderer {
namespace layer {
class Scene;
}

struct SceneInfo {
    Matrix4x4f view_projection_matrix;
    Matrix4x4f view_matrix;
    Matrix4x4f inverse_view_matrix;
    Matrix4x4f projection_matrix;
    Matrix4x4f inverse_projection_matrix;
    Matrix4x4f inverse_view_projection_matrix;
    Matrix4x4f old_view_projection_matrix;
};

struct Rect {
    float left;
    float top;
    float right;
    float bottom;
};

class TargetState;

template<typename T>
class DirectXResource : public RenderResource {
public:
    T* get_native_resource() const {
        return *(T**)((uintptr_t)this + sizeof(RenderResource));
    }

private:
};

class Texture : public RenderResource {
public:
    struct Desc {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t mip;
        uint32_t arr;
        uint32_t format; // DXGI_FORMAT/via.render.TextureFormat

        // rest dont care
    };

    Texture* clone();
    Texture* clone(uint32_t new_width, uint32_t new_height) {
        // Modifying description directly as dont have full definition of the Desc struct
        auto desc = get_desc();

        const auto old_width = desc->width;
        const auto old_height = desc->height;

        desc->width = new_width;
        desc->height = new_height;

        auto new_texture = clone();

        desc->width = old_width;
        desc->height = old_height;

        return new_texture;
    }

    Desc* get_desc() {
        return (Desc*)((uintptr_t)this + s_desc_offset);
    }

    DirectXResource<ID3D12Resource>* get_d3d12_resource_container() {
        return *(DirectXResource<ID3D12Resource>**)((uintptr_t)this + s_d3d12_resource_offset);
    }

private:
#if TDB_VER >= 73 || defined(SF6)
    static constexpr inline auto s_desc_offset = sizeof(RenderResource) + 0x18;
#else
    static constexpr inline auto s_desc_offset = sizeof(RenderResource) + sizeof(void*);
#endif

#if TDB_VER >= 73
    static constexpr inline auto s_d3d12_resource_offset = 0xE0;
#elif TDB_VER >= 71
#ifdef SF6
    // So because this discrepancy in SF6 is > 8 bytes (which is how much was added to RenderResource), trying to automate this
    // is a bit trickier so we can look into this later, and just hardcode it for now.
    static constexpr inline auto s_d3d12_resource_offset = 0xB8;
#elif defined(MHRISE)
    static constexpr inline auto s_d3d12_resource_offset = 0x98; // WHAT THE HECK!!!
#else
    static constexpr inline auto s_d3d12_resource_offset = 0xA0;
#endif
#elif TDB_VER == 70
    static constexpr inline auto s_d3d12_resource_offset = 0x98;
#elif TDB_VER == 69
    static constexpr inline auto s_d3d12_resource_offset = 0x98;
#else
    // TODO? might not be right offset (verified in DMC5)
    static constexpr inline auto s_d3d12_resource_offset = 0x98;
#endif
};

class DepthStencilView : public RenderResource {
};

class RenderTargetView : public RenderResource {
public:
    struct Desc {
        uint32_t format;
        uint32_t dimension;

        uint8_t unk_pad[0xC];
    };

    sdk::intrusive_ptr<RenderTargetView> clone();
    sdk::intrusive_ptr<RenderTargetView> clone(uint32_t new_width, uint32_t new_height);

    Desc& get_desc() {
        return m_desc;
    }

    sdk::intrusive_ptr<Texture>& get_texture_d3d12() const;
    sdk::intrusive_ptr<TargetState>& get_target_state_d3d12() const;

private:
    Desc m_desc;
};

static_assert(sizeof(RenderTargetView::Desc) == 0x14);

class TargetState : public RenderResource {
public:
    struct Desc;

    ID3D12Resource* get_native_resource_d3d12() const;
    sdk::intrusive_ptr<TargetState> clone() const;
    sdk::intrusive_ptr<TargetState> clone(const std::vector<std::array<uint32_t, 2>>& new_dimensions) const;

    Desc& get_desc() {
        return m_desc;
    }

    const Desc& get_desc() const {
        return m_desc;
    }

    uint32_t get_rtv_count() const {
        return m_desc.num_rtv;
    }

    sdk::intrusive_ptr<RenderTargetView> get_rtv(int32_t index) const {
        if (index < 0 || index >= get_rtv_count() || m_desc.rtvs == nullptr) {
            return nullptr;
        }
        
        return m_desc.rtvs[index];
    }

    void set_rtv(int32_t index, RenderTargetView* rtv) {
        if (index < 0 || index >= get_rtv_count() || m_desc.rtvs == nullptr) {
            return;
        }

        m_desc.rtvs[index] = rtv;
    }

public:
    struct Desc {
        sdk::intrusive_ptr<RenderTargetView>* rtvs;
        sdk::intrusive_ptr<DepthStencilView> dsv;
        uint32_t num_rtv;
        Rect rect;
        uint32_t flag;
    } m_desc;

    // more here but not needed... for now
};
#if TDB_VER >= 73
static_assert(offsetof(TargetState, m_desc) + offsetof(TargetState::Desc, num_rtv) == 0x28);
#elif TDB_VER > 67
#ifdef SF6
static_assert(offsetof(TargetState, m_desc) + offsetof(TargetState::Desc, num_rtv) == 0x28);
#else
static_assert(offsetof(TargetState, m_desc) + offsetof(TargetState::Desc, num_rtv) == 0x20);
#endif
#else
static_assert(offsetof(TargetState, m_desc) + offsetof(TargetState::Desc, num_rtv) == 0x28);
#endif

class Buffer : public RenderResource {
public:
    uint32_t m_size_in_bytes;
    uint32_t m_usage_type;
    uint32_t m_option_flags;
};

class ConstantBuffer : public Buffer {
public:
    uint32_t m_update_times;
};

class ConstantBufferDX12 : public ConstantBuffer {
public:
    void* get_desc() {
        return (void*)((uintptr_t)this + sizeof(ConstantBuffer));
    }
};

class RenderLayer : public sdk::ManagedObject {
public:
    RenderLayer* add_layer(::REType* layer_type, uint32_t priority, uint8_t offset = 0);
    void add_layer(RenderLayer* existing_layer) {
        m_layers.push_back(existing_layer);
        existing_layer->m_parent = this;
    }
    sdk::NativeArray<RenderLayer*>& get_layers();
    RenderLayer** find_layer(::REType* layer_type);
    std::tuple<RenderLayer*, RenderLayer**> find_layer_recursive(const ::REType* layer_type); // parent, type
    std::tuple<RenderLayer*, RenderLayer**> find_layer_recursive(std::string_view type_name); // parent, type
    std::vector<RenderLayer*> find_layers(::REType* layer_type);
    std::vector<layer::Scene*> find_all_scene_layers();
    std::vector<layer::Scene*> find_fully_rendered_scene_layers();
    
    RenderLayer* get_parent();
    void set_parent(RenderLayer* layer);
    RenderLayer* find_parent(::REType* layer_type);
    RenderLayer* clone(bool recursive = false);
    void clone(RenderLayer* other, bool recursive = false);
    void clone_layers(RenderLayer* other, bool recursive = false);

    ::sdk::renderer::TargetState* get_target_state(std::string_view name);

    ID3D12Resource* get_target_state_resource_d3d12(std::string_view name) {
        auto state = get_target_state(name);
        return state != nullptr ? state->get_native_resource_d3d12() : nullptr;
    }

#if TDB_VER >= 69
    static constexpr uint32_t DRAW_VTABLE_INDEX = 14;
#elif TDB_VER > 49
    static constexpr uint32_t DRAW_VTABLE_INDEX = 12;
#else
    static constexpr uint32_t DRAW_VTABLE_INDEX = 10;
#endif

    static constexpr uint32_t UPDATE_VTABLE_INDEX = DRAW_VTABLE_INDEX + 1;

    // only verified in RE8 and RE2.
#if TDB_VER >= 69
    static constexpr uint32_t NUM_PRIORITY_OFFSETS = 7;
#elif TDB_VER >= 66
    static constexpr uint32_t NUM_PRIORITY_OFFSETS = 6;
#else
    static constexpr uint32_t NUM_PRIORITY_OFFSETS = 0;
#endif

    void draw(void* render_context) {
        const auto vtable = *(void(***)(void*, void*))this;
        return vtable[DRAW_VTABLE_INDEX](this, render_context);
    }

    void update() {
        const auto vtable = *(void(***)(void*))this;
        return vtable[UPDATE_VTABLE_INDEX](this);
    }

public:
    uint32_t m_id;
    uint32_t m_render_output_id;
    uint32_t m_render_output_id_2;

#if TDB_VER <= 49
    sdk::renderer::RenderLayer* m_parent;
    sdk::NativeArray<sdk::renderer::RenderLayer*> m_layers;
    uint32_t m_priority;
#else
    uint32_t m_priority;
    uint32_t m_priority_offsets[NUM_PRIORITY_OFFSETS];
    sdk::renderer::RenderLayer* m_parent;
    sdk::NativeArray<sdk::renderer::RenderLayer*> m_layers;
#endif

    struct {
        void* DebugInfo;
        uint32_t LockCount;
        uint32_t RecursionCount;
        void* OwningThread;
        void* LockSemaphore;
        uintptr_t SpinCount;
    } m_cs;
    uint32_t m_version;
};

#if TDB_VER <= 49
static_assert(offsetof(RenderLayer, m_priority) == 0x48, "RenderLayer::m_priority offset is wrong");
static_assert(offsetof(RenderLayer, m_layers) == 0x38, "RenderLayer::m_layers offset is wrong");
static_assert(offsetof(RenderLayer, m_parent) == 0x30, "RenderLayer::m_parent offset is wrong");
#endif

namespace layer {
class Output : public sdk::renderer::RenderLayer {
public:
    // Not only grabs the scene view, but grabs a reference to it
    // so we can modify it.
    void*& get_present_state(); // via.render.OutputTargetState
    REManagedObject*& get_scene_view();

    void* get_output_target_d3d12() {
        return get_target_state_resource_d3d12("OutputTarget");
    }

    sdk::renderer::TargetState* get_present_output_state() {
        return get_target_state("PresentState");
    }
};

class Scene : public sdk::renderer::RenderLayer {
public:
    uint32_t get_view_id() const;
    RECamera* get_camera() const;
    RECamera* get_main_camera_if_possible() const;
    REManagedObject* get_mirror() const;
    bool is_enabled() const;

    bool has_main_camera() const {
        return get_main_camera_if_possible() != nullptr;
    }

    bool is_fully_rendered() const {
        return is_enabled() && get_mirror() == nullptr && has_main_camera();
    }

    sdk::renderer::SceneInfo* get_scene_info();
    sdk::renderer::SceneInfo* get_depth_distortion_scene_info();
    sdk::renderer::SceneInfo* get_filter_scene_info();
    sdk::renderer::SceneInfo* get_jitter_disable_scene_info();
    sdk::renderer::SceneInfo* get_jitter_disable_post_scene_info();
    sdk::renderer::SceneInfo* get_z_prepass_scene_info();

    Texture* get_depth_stencil();
    TargetState* get_motion_vectors_state();
    ID3D12Resource* get_depth_stencil_d3d12();

    ID3D12Resource* get_motion_vectors_d3d12() {
        return get_target_state_resource_d3d12("VelocityTarget");
    }

    ID3D12Resource* get_post_main_target_d3d12() {
        return get_target_state_resource_d3d12("PostMainTarget");
    }

    ID3D12Resource* get_hdr_target_d3d12() {
        return get_target_state_resource_d3d12("HDRTarget");
    }

    auto get_hdr_target() {
        return get_target_state("HDRTarget");
    }

    auto get_depth_target() {
        return get_target_state("DepthTarget");
    }

    ID3D12Resource* get_g_buffer_target_d3d12() {
        return get_target_state_resource_d3d12("GBufferTarget");
    }

    void set_lod_bias(float x, float y) {
#ifdef RE4
        *(float*)((uintptr_t)this + s_lod_bias_offset) = x;
        *(float*)((uintptr_t)this + s_lod_bias_offset + 4) = y;
#else
#endif
    }

private:
#ifdef RE4
    constexpr static auto s_lod_bias_offset = 0x1818;
#else // TODO
    constexpr static auto s_lod_bias_offset = 0;
#endif
};

class PrepareOutput : public sdk::renderer::RenderLayer {
public:
    sdk::renderer::TargetState* get_output_state() {
        return *(sdk::renderer::TargetState**)((uintptr_t)this + s_output_state_offset);
    }

    void set_output_state(sdk::renderer::TargetState* state) {
        state->add_ref();
        *(sdk::renderer::TargetState**)((uintptr_t)this + s_output_state_offset) = state;
    }

private:
    // Man I REALLY need a way of automatically finding this.
#if TDB_VER >= 73
    static constexpr inline auto s_output_state_offset = 0x128;
#elif TDB_VER >= 71
#ifdef MHRISE
    static constexpr inline auto s_output_state_offset = 0xF8;
#else
    // verify for other games, this is for RE4
    static constexpr inline auto s_output_state_offset = 0x108;
#endif
#elif TDB_VER >= 69
    static constexpr inline auto s_output_state_offset = 0xF8;
#else
    static constexpr inline auto s_output_state_offset = 0xE0; // Verified for DMC5
#endif
};

class Overlay : public sdk::renderer::RenderLayer {
public:
    sdk::intrusive_ptr<sdk::renderer::TargetState>& get_main_target_state() {
        return *(sdk::intrusive_ptr<sdk::renderer::TargetState>*)((uintptr_t)this + sizeof(sdk::renderer::RenderLayer) + sizeof(void*));
    }

    sdk::intrusive_ptr<sdk::renderer::TargetState>& get_main_depth_target_state() {
        return *(sdk::intrusive_ptr<sdk::renderer::TargetState>*)((uintptr_t)this + sizeof(sdk::renderer::RenderLayer));
    }

    sdk::renderer::Texture** get_b8g8r8a8_unorm_textures() {
        return (sdk::renderer::Texture**)((uintptr_t)this + s_b8g8r8a8_unorm_textures_offset);
    }

    sdk::renderer::Texture** get_r8g8b8a8_unorm_textures() {
        return (sdk::renderer::Texture**)((uintptr_t)this + s_r8g8b8a8_unorm_textures_offset);
    }

    sdk::renderer::TargetState** get_b8g8r8a8_unorm_target_states() {
        return (sdk::renderer::TargetState**)((uintptr_t)this + s_b8g8r8a8_unorm_target_state_offset);
    }

    sdk::renderer::TargetState** get_b8g8r8a8_unorm_target_only_states() {
        return (sdk::renderer::TargetState**)((uintptr_t)this + s_b8g8r8a8_unorm_target_only_state_offset);
    }

private:
#if TDB_VER >= 71
    // SF6/RE4
    static constexpr inline auto s_b8g8r8a8_unorm_textures_offset = 0x288;
    static constexpr inline auto s_r8g8b8a8_unorm_textures_offset = 0x260;
    static constexpr inline auto s_b8g8r8a8_unorm_target_state_offset = 0x1A0;
    static constexpr inline auto s_b8g8r8a8_unorm_target_only_state_offset = 0x1E0;
#else
    // verify
    static constexpr inline auto s_b8g8r8a8_unorm_textures_offset = 0x288;
    static constexpr inline auto s_r8g8b8a8_unorm_textures_offset = 0x260;
    static constexpr inline auto s_b8g8r8a8_unorm_target_state_offset = 0x1A0;
    static constexpr inline auto s_b8g8r8a8_unorm_target_only_state_offset = 0x1E0;
#endif
};

class PostEffect : public sdk::renderer::RenderLayer {
};
}

class RenderOutput {
public:
    sdk::NativeArray<sdk::renderer::layer::Scene*>& get_scene_layers() {
        return *(sdk::NativeArray<sdk::renderer::layer::Scene*>*)((uintptr_t)this + s_scene_layers_offset);
    }

private:
#if TDB_VER >= 71
    // verify for other games, this is for RE4
    static constexpr inline auto s_scene_layers_offset = 0x98;
#elif TDB_VER >= 70
    static constexpr inline auto s_scene_layers_offset = 0xB8;
#elif TDB_VER == 69
    static constexpr inline auto s_scene_layers_offset = 0x98;
#else
    static constexpr inline auto s_scene_layers_offset = 0x98; // TODO! VERIFY!
#endif
};

struct Fence {
    int32_t state{-2};
    uint32_t unk2{0};
    uint32_t unk3{0};
    uint32_t unk4{0x70};
};

namespace command {
struct Base {
    uint32_t t : 8;
    uint32_t size : 24;
    uint64_t priority{};
};

static_assert(sizeof(Base) == 0x10);

struct Clear : public Base {
    uint32_t clear_type{};
    sdk::renderer::TargetState* target; // target state/uav
    union {
        sdk::renderer::RenderTargetView* rtv;
        sdk::renderer::DepthStencilView* dsv;
    } view;

    float clear_color[4]{};
};

static_assert(sizeof(Clear) == 0x38);

struct FenceBase : public Base {
    const char* name{};
    sdk::renderer::Fence fence{};
};

static_assert(sizeof(FenceBase) == 0x28);

struct Fence : public FenceBase {
    bool wait{};
    bool begin;
    bool end;
    bool complete;
};

static_assert(sizeof(command::Fence) == 0x30);

struct CopyBase : public Base {
    sdk::renderer::RenderResource* src{};
    sdk::renderer::RenderResource* dst{};
    ::sdk::renderer::Fence fence{};
};

struct CopyTexture : public CopyBase {
    int32_t src_subresource{-1};
    int32_t dst_subresource{-1};
};

static_assert(sizeof(CopyBase) == 0x30);
static_assert(sizeof(CopyTexture) == 0x38);
}

class RenderContext {
public:
    void set_pipeline_state(PipelineState* state);

    // tgx and y are usually width and height
    void dispatch_ray(uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z, Fence& fence);
    void dispatch_32bit_constant(uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z, uint32_t constant, bool disable_uav_barrier);
    void dispatch(uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z, bool disable_uav_barrier);

    // via::render::command::TypeId, can change between engine versions
    command::Base* alloc(uint32_t t, uint32_t size);
    void clear_rtv(sdk::renderer::RenderTargetView* rtv, float color[4], bool delay = false);
    void clear_rtv(sdk::renderer::RenderTargetView* rtv, bool delay = false) {
        float color[4]{0.0f, 0.0f, 0.0f, 0.0f};
        clear_rtv(rtv, color, delay);
    }

    void copy_texture(Texture* dest, Texture* src, Fence& fence);
    void copy_texture(Texture* dest, Texture* src) {
        Fence fence{};
        copy_texture(dest, src, fence);
    }

public:
    uint32_t get_protect_frame() const {
        return *(uint32_t*)((uintptr_t)this + s_protect_frame_offset);
    }

    sdk::renderer::TargetState* get_render_target() const {
        return *(sdk::renderer::TargetState**)((uintptr_t)this + s_current_render_state_offset);
    }

    bool is_delay_enabled() const {
        return *(bool*)((uintptr_t)this + s_is_delay_enabled_offset);
    }

#if TDB_VER >= 69
    static constexpr inline auto s_protect_frame_offset = 0x68;
    static constexpr inline auto s_is_delay_enabled_offset = 0x7B;
    static constexpr inline auto s_current_render_state_offset = 0x98;
#else
    // verify
    static constexpr inline auto s_protect_frame_offset = 0x68;
    static constexpr inline auto s_is_delay_enabled_offset = 0x7B;
    static constexpr inline auto s_current_render_state_offset = 0x98;
#endif
};

class Renderer {
public:
    void* get_device() const {
        return *(void**)((uintptr_t)this + sizeof(void*)); // simple!
    }

    std::optional<uint32_t> get_render_frame() const;
    
    ConstantBuffer* get_constant_buffer(std::string_view name) const;

    ConstantBuffer* get_scene_info() const {
        return get_constant_buffer("SceneInfo");
    }

    ConstantBuffer* get_shadow_cast_info() const {
        return get_constant_buffer("ShadowCastInfo");
    }

    ConstantBuffer* get_environment_info() const {
        return get_constant_buffer("EnvironmentInfo");
    }

    ConstantBuffer* get_fog_parameter() const {
        return get_constant_buffer("FogParameter");
    }
};

Renderer* get_renderer();

void wait_rendering();
void begin_update_primitive();
void update_primitive();
void end_update_primitive();

void begin_rendering();
void end_rendering();

void add_scene_view(void* scene_view);
void remove_scene_view(void* scene_view);
RenderLayer* get_root_layer();
RenderLayer* find_layer(::REType* layer_type);

sdk::renderer::layer::Output* get_output_layer();

std::optional<Vector2f> world_to_screen(const Vector3f& world_pos);

ConstantBuffer* create_constant_buffer(void* desc);
TargetState* create_target_state(TargetState::Desc* desc);
Texture* create_texture(Texture::Desc* desc);
RenderTargetView* create_render_target_view(sdk::renderer::RenderResource* resource, void* desc);
}
}
