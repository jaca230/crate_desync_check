# cmake/CPMConfig.cmake

# ---------------------- CPM Package Registry ----------------------
# NOTE: Order matters! Dependencies should be declared in dependency order.
# NOTE: The name in package list matters! Dependencies use "Find{name}.cmake",
# so the name has to match the normal package export name for that package



set(CPM_PACKAGE_LIST
  analysis_pipeline_core       # needed for all analysis_pipeline libraries/plugins
  TBB                          # needed for analysis pipeline
  analysis_pipeline            # needed for all libraries/plugins that make internal pipelines
  unpacker_data_products_core  # needed for all unpackers libraries/plugins
  unpacker_data_products_nalu  # needed for all nalu unpackers libraries/plugins
  unpacker_stages_core         # needed for all unpacker stages libraries/plugins
  # unpacker_stages_nalu         # no additional libraries/plugins depend on this in this app
  midas_event_unpacker_plugin  # no additional libraries/plugins depend on this in this app
  # byte_stream_unpacker_plugin  # no additional libraries/plugins depend on this in this app
)

# ---------------------- analysis_pipeline_core ----------------------
set(analysis_pipeline_core_REPO   "jaca230/analysis_pipeline_core")
set(analysis_pipeline_core_TAG    "main")
set(analysis_pipeline_core_TARGETS
  analysis_pipeline::analysis_pipeline_core
  analysis_pipeline::spdlog_header_only
  analysis_pipeline::nlohmann_json_header_only
)
set(analysis_pipeline_core_OPTIONS
  "CMAKE_POSITION_INDEPENDENT_CODE ON"
)

# ---------------------- oneTBB ----------------------
set(TBB_REPO   "oneapi-src/oneTBB")
set(TBB_TAG    "v2021.12.0")  # Or latest stable
set(TBB_TARGET "") # Public Dependecy of Analysis Pipeline, no need to link
set(TBB_OPTIONS
  "TBB_TEST OFF;TBB_STRICT OFF;TBB_EXAMPLES OFF;CMAKE_POSITION_INDEPENDENT_CODE ON"
)


# ---------------------- analysis_pipeline ----------------------
set(analysis_pipeline_REPO   "jaca230/analysis_pipeline")
set(analysis_pipeline_TAG    "main")
set(analysis_pipeline_TARGET "analysis_pipeline::analysis_pipeline")
set(analysis_pipeline_OPTIONS
  "CMAKE_POSITION_INDEPENDENT_CODE ON"
  "BUILD_EXAMPLE_PLUGIN OFF"
)

# ---------------------- unpacker_data_products_core ----------------------
set(unpacker_data_products_core_REPO   "jaca230/unpacker_data_products_core")
set(unpacker_data_products_core_TAG    "main")
set(unpacker_data_products_core_TARGET "") # Dynamically linked
set(unpacker_data_products_coren_OPTIONS
  "CMAKE_POSITION_INDEPENDENT_CODE ON"
)


# ---------------------- midas_event_unpacker_plugin ----------------------
set(midas_event_unpacker_plugin_REPO   "jaca230/midas_event_unpacker_plugin")
set(midas_event_unpacker_plugin_TAG    "main")
set(midas_event_unpacker_plugin_TARGET "analysis_pipeline::midas_event_unpacker_plugin")
set(midas_event_unpacker_plugin_OPTIONS
  "CMAKE_POSITION_INDEPENDENT_CODE ON"
  "USE_BUNDLED_MIDAS ON"
)

# ---------------------- unpacker_data_products_nalu ----------------------
set(unpacker_data_products_nalu_REPO   "jaca230/unpacker_data_products_nalu")
set(unpacker_data_products_nalu_TAG    "main")
set(unpacker_data_products_nalu_TARGET "analysis_pipeline::unpacker_data_products_nalu")
set(unpacker_data_products_nalu_OPTIONS
  "CMAKE_POSITION_INDEPENDENT_CODE ON"
)

# ---------------------- unpacker_stages_core ----------------------
set(unpacker_stages_core_REPO   "jaca230/unpacker_stages_core")
set(unpacker_stages_core_TAG    "main")
set(unpacker_stages_core_TARGET "analysis_pipeline::unpacker_stages_core")
set(unpacker_stages_core_OPTIONS
  "CMAKE_POSITION_INDEPENDENT_CODE ON"
)

# ---------------------- unpacker_stages_nalu ----------------------
set(unpacker_stages_nalu_REPO   "jaca230/unpacker_stages_nalu")
set(unpacker_stages_nalu_TAG    "main")
set(unpacker_stages_nalu_TARGET "analysis_pipeline::unpacker_stages_nalu")
set(unpacker_stages_nalu_OPTIONS
  "CMAKE_POSITION_INDEPENDENT_CODE ON"
)

# ---------------------- byte_stream_unpacker_plugin ----------------------
set(byte_stream_unpacker_plugin_REPO   "jaca230/byte_stream_unpacker_plugin")
set(byte_stream_unpacker_plugin_TAG    "main")
set(byte_stream_unpacker_plugin_TARGET "analysis_pipeline::byte_stream_unpacker_plugin")
set(byte_stream_unpacker_plugin_OPTIONS
  "CMAKE_POSITION_INDEPENDENT_CODE ON"
)
