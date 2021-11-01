namespace gpu_text {

	static const uint32_t		pass_fullscreen = 0;

	namespace fullscreen_vert {

		struct gl_PerVertex {
			vec4				gl_Position;
			float				gl_PointSize;
			float				gl_ClipDistance;
			float				gl_CullDistance;
		};


	} // namespace fullscreen_vert

	namespace fullscreen_frag {

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

		static const uint32_t binding_sb_DebugGpuFontBuffer = 2; // Set 0, binding 2
		static const uint32_t binding_sb_DebugGpuFontEntries = 3; // Set 0, binding 3
		static const uint32_t binding_sb_DebugGPUFontDispatch = 4; // Set 0, binding 4
		static const uint32_t binding_sb_DebugGPUIndirect = 5; // Set 0, binding 5

	} // namespace fullscreen_frag

	namespace fullscreen {
		static const uint32_t layout_DebugGpuFontBuffer = 0;
		static const uint32_t layout_DebugGpuFontEntries = 1;
		static const uint32_t layout_DebugGPUFontDispatch = 2;
		static const uint32_t layout_DebugGPUIndirect = 3;
	}

	static const uint32_t		pass_calculate_dispatch = 1;

	namespace calculate_dispatch_comp {

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

		static const uint32_t binding_sb_DebugGpuFontBuffer = 2; // Set 0, binding 2
		static const uint32_t binding_sb_DebugGpuFontEntries = 3; // Set 0, binding 3
		static const uint32_t binding_sb_DebugGPUFontDispatch = 4; // Set 0, binding 4
		static const uint32_t binding_sb_DebugGPUIndirect = 5; // Set 0, binding 5

	} // namespace calculate_dispatch_comp

	namespace calculate_dispatch {
		static const uint32_t layout_DebugGpuFontBuffer = 0;
		static const uint32_t layout_DebugGpuFontEntries = 1;
		static const uint32_t layout_DebugGPUFontDispatch = 2;
		static const uint32_t layout_DebugGPUIndirect = 3;
	}

	static const uint32_t		pass_sprite = 2;

	namespace sprite_vert {

		struct DebugGPUFontDispatch {
			uvec4				dispatches;
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
		static const uint32_t binding_sb_DebugGPUFontDispatch = 4; // Set 0, binding 4
		static const uint32_t binding_sb_DebugGpuFontEntries = 3; // Set 0, binding 3
		static const uint32_t binding_sb_DebugGpuFontBuffer = 2; // Set 0, binding 2
		static const uint32_t binding_sb_DebugGPUIndirect = 5; // Set 0, binding 5

	} // namespace sprite_vert

	namespace sprite_frag {

		struct DebugGpuFontBuffer {
			uint				current_data_index;
			uint				current_entry_index;
			uint				padding1;
			uint				padding2;
			vec4				data;
		};

		struct LocalConstants {
			mat4				view_projection_matrix;
			mat4				projection_matrix_2d;
		};

		struct Local {
			LocalConstants				locals;
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

	} // namespace sprite_frag

	namespace sprite {
		static const uint32_t layout_Local = 0;
		static const uint32_t layout_DebugGpuFontBuffer = 1;
		static const uint32_t layout_DebugGpuFontEntries = 2;
		static const uint32_t layout_DebugGPUFontDispatch = 3;
		static const uint32_t layout_DebugGPUIndirect = 4;
	}

	static const uint32_t		pass_through = 3;

	namespace through_vert {

		struct gl_PerVertex {
			vec4				gl_Position;
			float				gl_PointSize;
			float				gl_ClipDistance;
			float				gl_CullDistance;
		};


	} // namespace through_vert

	namespace through_frag {

		static const uint32_t binding_tex_albedo = 1; // Set 0, binding 1

	} // namespace through_frag

	namespace through {
		static const uint32_t layout_albedo = 0;
	}
} //
