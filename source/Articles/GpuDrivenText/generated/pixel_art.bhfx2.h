namespace pixel_art {

	static hydra::gfx::ResourceListCreation tables[ 2 ];

	static const uint32_t		pass_sprite_forward = 0;
	static const uint32_t		pass_sky_color = 1;
	static const uint32_t		pass_count = 2;

	namespace sprite_forward {
		namespace vert {

			struct gl_PerVertex {
				vec4					gl_Position;
				float					gl_PointSize;
				float					gl_ClipDistance;
				float					gl_CullDistance;
			};

			struct LocalConstants {
				mat4					view_projection_matrix;
				mat4					projection_matrix_2d;
				uint					screen_width;
				uint					screen_height;
				uint					disable_non_uniform_ext;
				uint					pad30;
			};

			struct Local {
				LocalConstants					locals;
			};

			struct DebugGpuFontBuffer {
				uint					current_data_index;
				uint					current_entry_index;
				uint					padding1;
				uint					padding2;
				vec4					data;
			};

			struct DebugGPUStringEntry {
				float					x;
				float					y;
				uint					offset;
				uint					count;
			};

			struct DebugGpuFontEntries {
				DebugGPUStringEntry					entries;
			};

			struct DebugGPUFontDispatch {
				uvec4					dispatches;
			};

			struct DebugGPUIndirect {
				uint					vertex_count;
				uint					instance_count;
				uint					first_vertex;
				uint					first_instance;
				uint					pad00;
				uint					pad01;
				uint					pad02;
				uint					pad03;
			};

			static const uint32_t binding_cb_Local = 0; // Set 0, binding 0
			static const uint32_t binding_sb_DebugGpuFontBuffer = 2; // Set 0, binding 2
			static const uint32_t binding_sb_DebugGpuFontEntries = 3; // Set 0, binding 3
			static const uint32_t binding_sb_DebugGPUFontDispatch = 4; // Set 0, binding 4
			static const uint32_t binding_sb_DebugGPUIndirect = 5; // Set 0, binding 5

		} // namespace vert

		namespace frag {

			struct LocalConstants {
				mat4					view_projection_matrix;
				mat4					projection_matrix_2d;
				uint					screen_width;
				uint					screen_height;
				uint					disable_non_uniform_ext;
				uint					pad30;
			};

			struct Local {
				LocalConstants					locals;
			};

			static const uint32_t binding_cb_Local = 0; // Set 0, binding 0
			static const uint32_t binding_tex_textures = 10; // Set 1, binding 10
			static const uint32_t binding_tex_textures_3d = 10; // Set 1, binding 10

		} // namespace frag

		static const uint32_t layout_Local = 0;
		static const uint32_t layout_albedo = 1;
		static const uint32_t layout_DebugGpuFontBuffer = 2;
		static const uint32_t layout_DebugGpuFontEntries = 3;

		struct Table {
			Table& reset() {
				rlc = &tables[ pass_sprite_forward ];
				rlc->reset();
				return *this;
			}
			Table& set_Local( hydra::gfx::Buffer* buffer ) {
				rlc->buffer( buffer->handle, layout_Local );
				return *this;
			}
			Table& set_albedo( hydra::gfx::Texture* texture ) {
				rlc->texture( texture->handle, layout_albedo );
				return *this;
			}
			Table& set_DebugGpuFontBuffer( hydra::gfx::Buffer* buffer ) {
				rlc->buffer( buffer->handle, layout_DebugGpuFontBuffer );
				return *this;
			}
			Table& set_DebugGpuFontEntries( hydra::gfx::Buffer* buffer ) {
				rlc->buffer( buffer->handle, layout_DebugGpuFontEntries );
				return *this;
			}

			hydra::gfx::ResourceListCreation* rlc;
		}; // struct Table


		static Table& table() { static Table s_table; return s_table; }

	} // pass sprite_forward

	namespace sky_color {
		namespace vert {

			struct gl_PerVertex {
				vec4					gl_Position;
				float					gl_PointSize;
				float					gl_ClipDistance;
				float					gl_CullDistance;
			};

			static const uint32_t binding_tex_textures = 10; // Set 1, binding 10
			static const uint32_t binding_tex_textures_3d = 10; // Set 1, binding 10

		} // namespace vert

		namespace frag {

			static const uint32_t binding_tex_textures = 10; // Set 1, binding 10
			static const uint32_t binding_tex_textures_3d = 10; // Set 1, binding 10

		} // namespace frag


		struct Table {
			Table& reset() {
				rlc = &tables[ pass_sky_color ];
				rlc->reset();
				return *this;
			}

			hydra::gfx::ResourceListCreation* rlc;
		}; // struct Table


		static Table& table() { static Table s_table; return s_table; }

		static const uint32_t layout_textures_3d = 0;
	} // pass sky_color


} // shader pixel_art
