namespace debug_rendering {

	static const uint32_t		pass_InstancedLines3D = 0;

	namespace InstancedLines3D_vert {

		struct gl_PerVertex {
			vec4				gl_Position;
			float				gl_PointSize;
			float				gl_ClipDistance;
			float				gl_CullDistance;
		};

		struct LocalConstants {
			mat4				view_projection_matrix;
			mat4				projection_matrix;
			vec4				resolution;
			vec4				data;
		};

		static const uint32_t binding_cb_LocalConstants = 0; // Set 0, binding 0

	} // namespace InstancedLines3D_vert

	namespace InstancedLines3D_frag {

		struct LocalConstants {
			mat4				view_projection_matrix;
			mat4				projection_matrix;
			vec4				resolution;
			vec4				data;
		};

		static const uint32_t binding_cb_LocalConstants = 0; // Set 0, binding 0

	} // namespace InstancedLines3D_frag

} //
