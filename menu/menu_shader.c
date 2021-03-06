/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include <compat/strl.h>
#include <retro_assert.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "menu_driver.h"
#include "menu_shader.h"
#include "../file_path_special.h"
#include "../configuration.h"
#include "../runloop.h"
#include "../verbosity.h"

/**
 * menu_shader_manager_init:
 *
 * Initializes shader manager.
 **/
void menu_shader_manager_init(menu_handle_t *menu)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;
   config_file_t *conf         = NULL;
   settings_t *settings        = config_get_ptr();
   const char *config_path     = config_get_active_path();

   if (!menu)
      return;

   menu_driver_ctl(RARCH_MENU_CTL_SHADER_GET,
         &shader);

   /* In a multi-config setting, we can't have
    * conflicts on menu.cgp/menu.glslp. */
   if (config_path)
   {
      fill_pathname_base_ext(menu->default_glslp, config_path,
            file_path_str(FILE_PATH_GLSLP_EXTENSION),
            sizeof(menu->default_glslp));

      fill_pathname_base_ext(menu->default_cgp, config_path,
            file_path_str(FILE_PATH_CGP_EXTENSION),
            sizeof(menu->default_cgp));

      fill_pathname_base_ext(menu->default_slangp, config_path,
            file_path_str(FILE_PATH_SLANGP_EXTENSION),
            sizeof(menu->default_slangp));
   }
   else
   {
      strlcpy(menu->default_glslp, "menu.glslp",
            sizeof(menu->default_glslp));
      strlcpy(menu->default_cgp, "menu.cgp",
            sizeof(menu->default_cgp));
      strlcpy(menu->default_slangp, "menu.slangp",
            sizeof(menu->default_slangp));
   }

   switch (msg_hash_to_file_type(msg_hash_calculate(
               path_get_extension(settings->path.shader))))
   {
      case FILE_TYPE_SHADER_PRESET_GLSLP:
      case FILE_TYPE_SHADER_PRESET_CGP:
      case FILE_TYPE_SHADER_PRESET_SLANGP:
         conf = config_file_new(settings->path.shader);
         if (conf)
         {
            if (video_shader_read_conf_cgp(conf, shader))
            {
               video_shader_resolve_relative(shader,
                     settings->path.shader);
               video_shader_resolve_parameters(conf, shader);
            }
            config_file_free(conf);
         }
         break;
      case FILE_TYPE_SHADER_GLSL:
      case FILE_TYPE_SHADER_CG:
      case FILE_TYPE_SHADER_SLANG:
         strlcpy(shader->pass[0].source.path, settings->path.shader,
               sizeof(shader->pass[0].source.path));
         shader->passes = 1;
         break;
      default:
         {
            char preset_path[PATH_MAX_LENGTH] = {0};
            const char *shader_dir            = 
               *settings->directory.video_shader ?
               settings->directory.video_shader : settings->directory.system;

            fill_pathname_join(preset_path, shader_dir,
                  "menu.glslp", sizeof(preset_path));
            conf = config_file_new(preset_path);

            if (!conf)
            {
               fill_pathname_join(preset_path, shader_dir,
                     "menu.cgp", sizeof(preset_path));
               conf = config_file_new(preset_path);
            }

            if (!conf)
            {
               fill_pathname_join(preset_path, shader_dir,
                     "menu.slangp", sizeof(preset_path));
               conf = config_file_new(preset_path);
            }

            if (conf)
            {
               if (video_shader_read_conf_cgp(conf, shader))
               {
                  video_shader_resolve_relative(shader, preset_path);
                  video_shader_resolve_parameters(conf, shader);
               }
               config_file_free(conf);
            }
         }
         break;
   }
#endif
}

/**
 * menu_shader_manager_set_preset:
 * @shader                   : Shader handle.   
 * @type                     : Type of shader.
 * @preset_path              : Preset path to load from.
 *
 * Sets shader preset.
 **/
void menu_shader_manager_set_preset(struct video_shader *shader,
      unsigned type, const char *preset_path)
{
#ifdef HAVE_SHADER_MANAGER
   config_file_t *conf         = NULL;
   bool refresh                = false;
   settings_t *settings        = config_get_ptr();

   settings->video.shader_enable = false;

   if (!video_driver_set_shader((enum rarch_shader_type)type, preset_path))
      return;

   /* Makes sure that we use Menu Preset shader on driver reinit.
    * Only do this when the cgp actually works to avoid potential errors. */
   strlcpy(settings->path.shader,
         preset_path ? preset_path : "",
         sizeof(settings->path.shader));
   settings->video.shader_enable = true;

   if (!preset_path || !shader)
      return;

   /* Load stored Preset into menu on success. 
    * Used when a preset is directly loaded.
    * No point in updating when the Preset was 
    * created from the menu itself. */
   conf = config_file_new(preset_path);

   if (!conf)
      return;

   RARCH_LOG("Setting Menu shader: %s.\n", preset_path);

   if (video_shader_read_conf_cgp(conf, shader))
   {
      video_shader_resolve_relative(shader, preset_path);
      video_shader_resolve_parameters(conf, shader);
   }
   config_file_free(conf);

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
#endif
}

/**
 * menu_shader_manager_save_preset:
 * @basename                 : basename of preset
 * @apply                    : immediately set preset after saving
 *
 * Save a shader preset to disk.
 **/
bool menu_shader_manager_save_preset(
      const char *basename, bool apply, bool fullpath)
{
#ifdef HAVE_SHADER_MANAGER
   char buffer[PATH_MAX_LENGTH]           = {0};
   char config_directory[PATH_MAX_LENGTH] = {0};
   char preset_path[PATH_MAX_LENGTH]      = {0};
   unsigned d, type                       = RARCH_SHADER_NONE;
   const char *dirs[3]                    = {0};
   config_file_t *conf                    = NULL;
   bool ret                               = false;
   struct video_shader *shader            = NULL;
   global_t *global                       = global_get_ptr();
   settings_t *settings                   = config_get_ptr();
   menu_handle_t *menu                    = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
   {
      RARCH_ERR("Cannot save shader preset, menu handle"
            " is not initialized.\n");
      return false;
   }

   menu_driver_ctl(RARCH_MENU_CTL_SHADER_GET,
         &shader);

   if (!shader)
      return false;

   type = menu_shader_manager_get_type(shader);

   if (type == RARCH_SHADER_NONE)
      return false;

   *config_directory = '\0';

   if (basename)
   {
      strlcpy(buffer, basename, sizeof(buffer));

      /* Append extension automatically as appropriate. */
      if (     !strstr(basename, file_path_str(FILE_PATH_CGP_EXTENSION)) 
            && !strstr(basename, file_path_str(FILE_PATH_GLSLP_EXTENSION))
            && !strstr(basename, file_path_str(FILE_PATH_SLANGP_EXTENSION)))
      {
         switch (type)
         {
            case RARCH_SHADER_GLSL:
               strlcat(buffer,
                     file_path_str(FILE_PATH_GLSLP_EXTENSION),
                     sizeof(buffer));
               break;
            case RARCH_SHADER_SLANG:
               strlcat(buffer,
                     file_path_str(FILE_PATH_SLANGP_EXTENSION),
                     sizeof(buffer));
               break;
            case RARCH_SHADER_CG:
               strlcat(buffer,
                     file_path_str(FILE_PATH_CGP_EXTENSION),
                     sizeof(buffer));
               break;
         }
      }
   }
   else
   {
      const char *conf_path = NULL;
      switch (type)
      {
         case RARCH_SHADER_GLSL:
            conf_path = menu->default_glslp;
            break;

         case RARCH_SHADER_SLANG:
            conf_path = menu->default_slangp;
            break;

         default:
         case RARCH_SHADER_CG:
            conf_path = menu->default_cgp;
            break;
      }
      strlcpy(buffer, conf_path, sizeof(buffer));
   }

   if (!string_is_empty(global->path.config))
      fill_pathname_basedir(
            config_directory,
            global->path.config,
            sizeof(config_directory));

   if (!fullpath)
   {
      dirs[0] = settings->directory.video_shader;
      dirs[1] = settings->directory.menu_config;
      dirs[2] = config_directory;
   }

   if (!(conf = (config_file_t*)config_file_new(NULL)))
      return false;
   video_shader_write_conf_cgp(conf, shader);

   if (!fullpath)
   {
      for (d = 0; d < ARRAY_SIZE(dirs); d++)
      {
         if (!*dirs[d])
            continue;

         fill_pathname_join(preset_path, dirs[d],
               buffer, sizeof(preset_path));

         if (config_file_write(conf, preset_path))
         {
            RARCH_LOG("Saved shader preset to %s.\n", preset_path);
            if (apply)
               menu_shader_manager_set_preset(NULL, type, preset_path);
            ret = true;
            break;
         }
         else
            RARCH_LOG("Failed writing shader preset to %s.\n", preset_path);
      }
   }
   else
   {
      if (!string_is_empty(basename))
         strlcpy(preset_path, buffer, sizeof(preset_path));
         if (config_file_write(conf, preset_path))
         {
            RARCH_LOG("Saved shader preset to %s.\n", preset_path);
            if (apply)
               menu_shader_manager_set_preset(NULL, type, preset_path);
            ret = true;
         }
         else
            RARCH_LOG("Failed writing shader preset to %s.\n", preset_path);
   }

   config_file_free(conf);
   if (!ret)
   {
      RARCH_ERR("Failed to save shader preset. Make sure config directory"
            " and/or shader dir are writable.\n");
      return false;
   }
   else
      return true;
#endif
}

/**
 * menu_shader_manager_get_type:
 * @shader                   : shader handle     
 *
 * Gets type of shader.
 *
 * Returns: type of shader. 
 **/
unsigned menu_shader_manager_get_type(const struct video_shader *shader)
{
#ifndef HAVE_SHADER_MANAGER
   return RARCH_SHADER_NONE;
#else
   /* All shader types must be the same, or we cannot use it. */
   unsigned i = 0, type = 0;

   if (!shader)
      return RARCH_SHADER_NONE;

   for (i = 0; i < shader->passes; i++)
   {
      enum rarch_shader_type pass_type = 
         video_shader_parse_type(shader->pass[i].source.path,
            RARCH_SHADER_NONE);

      switch (pass_type)
      {
         case RARCH_SHADER_CG:
         case RARCH_SHADER_GLSL:
         case RARCH_SHADER_SLANG:
            if (type == RARCH_SHADER_NONE)
               type = pass_type;
            else if (type != pass_type)
               return RARCH_SHADER_NONE;
            break;
         default:
            return RARCH_SHADER_NONE;
      }
   }

   return type;
#endif
}

/**
 * menu_shader_manager_apply_changes:
 *
 * Apply shader state changes.
 **/
void menu_shader_manager_apply_changes(void)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned shader_type;
   struct video_shader *shader = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SHADER_GET,
         &shader);

   if (!shader)
      return;

   shader_type = menu_shader_manager_get_type(shader);

   if (shader->passes && shader_type != RARCH_SHADER_NONE)
   {
      menu_shader_manager_save_preset(NULL, true, false);
      return;
   }

   /* Fall-back */
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
   shader_type = video_shader_parse_type("", DEFAULT_SHADER_TYPE);
#endif

   if (shader_type == RARCH_SHADER_NONE)
   {
#if defined(HAVE_GLSL)
      shader_type = RARCH_SHADER_GLSL;
#elif defined(HAVE_CG) || defined(HAVE_HLSL)
      shader_type = RARCH_SHADER_CG;
#elif defined(HAVE_VULKAN)
      shader_type = RARCH_SHADER_SLANG;
#endif
   }
   menu_shader_manager_set_preset(NULL, shader_type, NULL);
#endif
}
