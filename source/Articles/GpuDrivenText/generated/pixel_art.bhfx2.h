namespace pixel_art {

	static const uint32_t		pass_fat_sprite = 0;

	namespace fat_sprite_vert {

		struct gl_PerVertex {
			vec4				gl_Position;
			float				gl_PointSize;
			float				gl_ClipDistance;
			float				gl_CullDistance;
		};

		struct LocalConstants {
			mat4				view_projection_matrix;
			mat4				projection_matrix_2d;
		};

		struct Local {
			LocalConstants				locals;
		};

		struct DebugGpuFontBuffer {
			uint				current_data_index;
			uint				current_entry_index;
			uint				padding1;
			uint				padding2;
			vec4				data;
		};

		struct DebugGPUStringEntry {
			float				x;
			float				y;
			uint				offset;
			uint				count;
		};

		struct DebugGpuFontEntries {
			DebugGPUStringEntry				entries;
		};

		struct DebugGPUFontDispatch {
			uvec4				dispatches;
		};

		struct DebugGPUIndirect {
			uint				vertex_count;
			uint				instance_count;
			uint				first_vertex;
			uint				first_instance;
			uint				pad00;
			uint				pad01;
			uint				pad02;
			uint				pad03;
		};

		static const uint32_t binding_cb_Local = 0; // Set 0, binding 0
		static const uint32_t binding_sb_DebugGpuFontBuffer = 2; // Set 0, binding 2
		static const uint32_t binding_sb_DebugGpuFontEntries = 3; // Set 0, binding 3
		static const uint32_t binding_sb_DebugGPUFontDispatch = 4; // Set 0, binding 4
		static const uint32_t binding_sb_DebugGPUIndirect = 5; // Set 0, binding 5

	} // namespace fat_sprite_vert

	namespace fat_sprite_frag {

		struct LocalConstants {
			mat4				view_projection_matrix;
			mat4				projection_matrix_2d;
		};

		struct Local {
			LocalConstants				locals;
		};

		static const uint32_t binding_cb_Local = 0; // Set 0, binding 0
		static const uint32_t binding_tex_albedo = 1; // Set 0, binding 1

	} // namespace fat_sprite_frag

	namespace fat_sprite {
		static const uint32_t layout_Local = 0;
		static const uint32_t layout_albedo = 1;
		static const uint32_t layout_DebugGpuFontBuffer = 2;
		static const uint32_t layout_DebugGpuFontEntries = 3;
		static const uint32_t layout_DebugGPUFontDispatch = 4;
		static const uint32_t layout_DebugGPUIndirect = 5;
	}
} //
