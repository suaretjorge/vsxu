#include <vsx_module.h>

#include <texture/vsx_texture.h>
#include <bitmap/generators/vsx_bitmap_generator_plasma.h>

class module_texture_texgen_plasma : public vsx_module
{
  // in - function
  vsx_module_param_float4* col_amp_in;
  vsx_module_param_float4* col_ofs_in;

  // in - function - period
  vsx_module_param_float3* r_period_in;
  vsx_module_param_float3* g_period_in;
  vsx_module_param_float3* b_period_in;
  vsx_module_param_float3* a_period_in;

  // in - function - ofs
  vsx_module_param_float3* r_ofs_in;
  vsx_module_param_float3* g_ofs_in;
  vsx_module_param_float3* b_ofs_in;
  vsx_module_param_float3* a_ofs_in;

  // in - options
  vsx_module_param_int* size_in;

  // in - rendering_hints
  vsx_module_param_int* min_mag_filter_in;
  vsx_module_param_int* anisotropic_filtering_in;
  vsx_module_param_int* mipmaps_in;
  vsx_module_param_int* mipmap_min_filter_in;

  // out
  vsx_module_param_texture* texture_out;

  // internal
  vsx_texture<>* texture = 0x0;
  vsx_texture<>* texture_old = 0x0;

  bool first = true;
  bool worker_running = false;
  bool delete_texture_old = false;

public:

  void module_info(vsx_module_info* info)
  {
    info->identifier =
      "texture;generators;plasma"
    ;

    info->description =
      "Generates a Sin-plasma bitmap"
    ;

    info->in_param_spec =
        "function:complex{"
          "col_amp:float4,"
          "col_ofs:float4,"
          "period:complex{"
            "r_period:float3,"
            "g_period:float3,"
            "b_period:float3,"
            "a_period:float3"
          "},"
          "ofs:complex{"
            "r_ofs:float3,"
            "g_ofs:float3,"
            "b_ofs:float3,"
            "a_ofs:float3"
          "}"
        "},"
        "options:complex{"
          "size:enum?8x8|16x16|32x32|64x64|128x128|256x256|512x512|1024x1024&nc=1"
        "},"
        "rendering_hints:complex{"
          "min_mag_filter:enum?nearest|linear&nc=1,"
          "anisotropic_filter:enum?no|yes&nc=1,"
          "mipmaps:enum?no|yes&nc=1,"
          "mipmap_min_filter:enum?nearest|linear&nc=1"
        "},"
    ;

    info->out_param_spec =
      "texture:texture";

    info->component_class =
      "texture";
  }

  void declare_params(vsx_module_param_list& in_parameters, vsx_module_param_list& out_parameters)
  {
    loading_done = true;

    // function
    col_amp_in = (vsx_module_param_float4*)in_parameters.create(VSX_MODULE_PARAM_ID_FLOAT4,"col_amp");
    col_amp_in->set(1.0f, 0);
    col_amp_in->set(1.0f, 1);
    col_amp_in->set(1.0f, 2);
    col_amp_in->set(1.0f, 3);
    col_ofs_in = (vsx_module_param_float4*)in_parameters.create(VSX_MODULE_PARAM_ID_FLOAT4,"col_ofs");

    r_period_in = (vsx_module_param_float3*)in_parameters.create(VSX_MODULE_PARAM_ID_FLOAT3,"r_period");
    r_period_in->set(1.0f, 0);
    r_period_in->set(1.0f, 1);

    g_period_in = (vsx_module_param_float3*)in_parameters.create(VSX_MODULE_PARAM_ID_FLOAT3,"g_period");
    g_period_in->set(1.0f, 0);
    g_period_in->set(1.0f, 1);

    b_period_in = (vsx_module_param_float3*)in_parameters.create(VSX_MODULE_PARAM_ID_FLOAT3,"b_period");
    a_period_in = (vsx_module_param_float3*)in_parameters.create(VSX_MODULE_PARAM_ID_FLOAT3,"a_period");

    r_ofs_in = (vsx_module_param_float3*)in_parameters.create(VSX_MODULE_PARAM_ID_FLOAT3,"r_ofs");
    g_ofs_in = (vsx_module_param_float3*)in_parameters.create(VSX_MODULE_PARAM_ID_FLOAT3,"g_ofs");
    b_ofs_in = (vsx_module_param_float3*)in_parameters.create(VSX_MODULE_PARAM_ID_FLOAT3,"b_ofs");
    a_ofs_in = (vsx_module_param_float3*)in_parameters.create(VSX_MODULE_PARAM_ID_FLOAT3,"a_ofs");

    // options
    size_in = (vsx_module_param_int*)in_parameters.create(VSX_MODULE_PARAM_ID_INT,"size");
    size_in->set(4);

    // rendering_hints
    min_mag_filter_in = (vsx_module_param_int*)in_parameters.create(VSX_MODULE_PARAM_ID_INT, "min_mag_filter");
    min_mag_filter_in->set(1); // linear

    anisotropic_filtering_in = (vsx_module_param_int*)in_parameters.create(VSX_MODULE_PARAM_ID_INT, "anisotropic_filter");

    mipmaps_in = (vsx_module_param_int*)in_parameters.create(VSX_MODULE_PARAM_ID_INT, "mipmaps");

    mipmap_min_filter_in = (vsx_module_param_int*)in_parameters.create(VSX_MODULE_PARAM_ID_INT, "mipmap_min_filter");
    mipmap_min_filter_in->set(1);

    // out
    texture_out = (vsx_module_param_texture*)out_parameters.create(VSX_MODULE_PARAM_ID_TEXTURE,"texture");
  }

  void run()
  {
    if (worker_running && !texture->texture->bitmap->data_ready)
      return;

    if (worker_running && texture->texture->bitmap->data_ready)
    {
      texture_out->set(texture);
      worker_running = false;
      if (texture_old)
      {
        vsx_texture_gl_cache::get_instance()->destroy( texture_old->texture );
        delete texture_old;
        texture_old = 0x0;
      }
      return;
    }

    req(param_updates);
    param_updates = 0;

    uint64_t hint = 0;
    hint |= vsx_texture_gl::anisotropic_filtering_hint * anisotropic_filtering_in->get();
    hint |= vsx_texture_gl::generate_mipmaps_hint * mipmaps_in->get();
    hint |= vsx_texture_gl::linear_interpolate_hint * min_mag_filter_in->get();
    hint |= vsx_texture_gl::mipmap_linear_interpolate_hint * mipmap_min_filter_in->get();

    vsx_string<> cache_handle = vsx_bitmap_generator_plasma::generate_cache_handle(
        vsx_vector2f(r_period_in->get(0), r_period_in->get(1)),
        vsx_vector2f(g_period_in->get(0), g_period_in->get(1)),
        vsx_vector2f(b_period_in->get(0), b_period_in->get(1)),
        vsx_vector2f(a_period_in->get(0), a_period_in->get(1)),
        vsx_vector2f(r_ofs_in->get(0), r_ofs_in->get(1)),
        vsx_vector2f(g_ofs_in->get(0), g_ofs_in->get(1)),
        vsx_vector2f(b_ofs_in->get(0), b_ofs_in->get(1)),
        vsx_vector2f(a_ofs_in->get(0), a_ofs_in->get(1)),
        vsx_colorf(col_amp_in->get(0), col_amp_in->get(1), col_amp_in->get(2), col_amp_in->get(3)),
        vsx_colorf(col_ofs_in->get(0), col_ofs_in->get(1), col_ofs_in->get(2), col_ofs_in->get(3)),
        size_in->get()
      );

    if (vsx_texture_gl_cache::get_instance()->has(cache_handle, 0, hint))
    {
      if (!texture)
        texture = new vsx_texture<>;

      texture->texture = vsx_texture_gl_cache::get_instance()->aquire(cache_handle, engine->filesystem, false, 0, hint, false );
      texture_out->set(texture);
      return;
    }

    if (texture)
      texture_old = texture;

    texture = new vsx_texture<>;

    texture->texture = vsx_texture_gl_cache::get_instance()->create(cache_handle, 0, hint);
    texture->texture->bitmap->filename = cache_handle;

    vsx_bitmap_generator_plasma::generate_thread(
          texture->texture->bitmap,
          vsx_vector2f(r_period_in->get(0), r_period_in->get(1)),
          vsx_vector2f(g_period_in->get(0), g_period_in->get(1)),
          vsx_vector2f(b_period_in->get(0), b_period_in->get(1)),
          vsx_vector2f(a_period_in->get(0), a_period_in->get(1)),
          vsx_vector2f(r_ofs_in->get(0), r_ofs_in->get(1)),
          vsx_vector2f(g_ofs_in->get(0), g_ofs_in->get(1)),
          vsx_vector2f(b_ofs_in->get(0), b_ofs_in->get(1)),
          vsx_vector2f(a_ofs_in->get(0), a_ofs_in->get(1)),
          vsx_colorf(col_amp_in->get(0), col_amp_in->get(1), col_amp_in->get(2), col_amp_in->get(3)),
          vsx_colorf(col_ofs_in->get(0), col_ofs_in->get(1), col_ofs_in->get(2), col_ofs_in->get(3)),
          size_in->get()
    );

    worker_running = true;
  }

  void on_delete()
  {
    vsx_thread_pool::instance()->wait_all();

    if (texture_old)
    {
      vsx_texture_gl_cache::get_instance()->destroy( texture_old->texture );
      delete texture_old;
    }
    if (texture)
    {
      vsx_texture_gl_cache::get_instance()->destroy( texture->texture );
      delete texture;
    }
  }
};

