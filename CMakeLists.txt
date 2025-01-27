# WARNING: this is in development and should not yet be used to replace automake

# before starting the project, set gcc/g++ as default
find_program(CMAKE_C_COMPILER NAMES $ENV{CC} gcc PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER NAMES $ENV{CXX} g++ PATHS ENV PATH NO_DEFAULT_PATH)

# project info
project(freesurfer)
cmake_minimum_required(VERSION 3.5)
enable_language(C CXX Fortran)

# a few build options
option(BUILD_GUIS "Build GUIs" ON)
option(NMR_INSTALL "Install NMR files" OFF)
if(APPLE)
  option(BUILD_OSX_TK_GUIS "Compile OSX Tk GUIs" OFF)
endif()

# enable ctest
enable_testing()

# include our custom cmake functions (and others)
include(cmake/functions.cmake)
include(TestBigEndian)
include(CheckFunctionExists)

# prevents itk from importing as a system
set(CMAKE_NO_SYSTEM_FROM_IMPORTED TRUE)

# xxd is used to generate the helptext headers - make sure it exists
find_program(XXD xxd)
if(NOT XXD)
  message(FATAL_ERROR "The xxd program is required to build freesurfer")
endif()


# --------------------------------------------------
#   library dependencies and third-party packages
# --------------------------------------------------

# Most of the packages required by freesurfer are located by custom find-modules stored in the
# 'cmake' subdir. The find-modules expect each package to be installed under a common
# path defined by FS_PACKAGES_DIR. On Martinos machines, this variable automatically defaults
# to /usr/pubsw/packages, but external developers must provide this path manually. External developers
# can run the packages/build_packages.py script to compile the dependencies locally. If a package
# is not found under FS_PACKAGES_DIR, cmake will continue to look through the default search paths.
# Additionally, alternative paths to package installs can be specified with the <PACKAGE>_DIR variables

if(NOT FS_PACKAGES_DIR)
  if(EXISTS /usr/pubsw/packages)
    # if the user is at Martinos, default to /usr/pubsw/packages
    set(FS_PACKAGES_DIR /usr/pubsw/packages)
  else()
    if(APPLE)
      set(FS_PACKAGES_URL "http://surfer.nmr.mgh.harvard.edu/pub/data/fspackages/prebuilt/osx10.11-packages.tar.gz")
    else()
      set(FS_PACKAGES_URL "http://surfer.nmr.mgh.harvard.edu/pub/data/fspackages/prebuilt/centos7-packages.tar.gz")
    endif()
    # setting FS_PACKAGES_DIR is required if the user is outside Martinos
    message(FATAL_ERROR "External FreeSurfer developers must provide a valid FS_PACKAGES_DIR path! "
                        "Required FS dependencies can be built locally by running:\n"
                        "    packages/build_packages.sh \"/desired/packages/install/dir\"\n"
                        "Or they can be downloaded from here:\n"
                        "    ${FS_PACKAGES_URL}\n"
                        "After the dependencies have been setup, cmake should be configured with:\n"
                        "    -DFS_PACKAGES_DIR=\"/path/to/fs-packages\"")
  endif()
endif()

# all custom find-modules are stored in the cmake subdir
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# -------- zlib --------
find_package(ZLIB REQUIRED)
add_definitions(-DHAVE_ZLIB)

if(NOT APPLE)

  # -------- gfortran --------
  find_library(GFORTRAN_LIBRARIES HINTS /usr/lib/gcc/x86_64-linux-gnu/4.8 NAMES libgfortran.a)

  # -------- blas and lapack --------
  find_library(BLAS_LIBRARIES NAMES libblas.a)
  find_library(LAPACK_LIBRARIES NAMES liblapack.a)

endif()

# -------- jpeg --------
find_package(JPEG REQUIRED)

# -------- tiff --------
find_package(TIFF REQUIRED)

# -------- expat --------
find_package(Expat REQUIRED)

# -------- xml2 --------
find_package(XML2 REQUIRED)

# -------- netcdf --------
find_package(NetCDF REQUIRED)

# -------- minc --------
find_package(MINC REQUIRED)

# -------- tetgen --------
find_package(Tetgen REQUIRED)

# -------- petsc --------
find_package(PETSC)

# -------- itk --------
if(NOT ITK_DIR)
  set(ITK_DIR ${FS_PACKAGES_DIR}/itk/5.0.0)
endif()
find_package(ITK HINTS ${ITK_DIR} REQUIRED)
add_definitions(-DHAVE_ITK_LIBS)

# -------- boost --------
set(BOOST_ROOT ${FS_PACKAGES_DIR}/boost/1.53)
find_package(Boost COMPONENTS program_options)

# -------- ann --------
# ANN is only used by mris_resample
find_package(ANN)

# -------- gts --------
# only used by mris_decimate and mris_decimate_gui
find_package(GTS)

# -------- openMP --------
find_package(OpenMP)
if(OPENMP_FOUND)
  add_definitions(-DHAVE_OPENMP)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} ${OpenMP_Fortran_FLAGS}")
endif()

# -------- openGL --------
find_package(OpenGL)
if(OPENGL_FOUND)
  add_definitions(-DHAVE_OPENGL)
  # since we have OpenGL, make sure we have glut too!
  find_package(GLUT REQUIRED)
  add_definitions(-DHAVE_GLUT_LIBS)
  find_package(X11 REQUIRED)
endif()

# -------- openCV --------
find_package(OpenCV)

# -------- openCL --------
find_package(OpenCL)

# search for GUI libraries
if(BUILD_GUIS)

  # -------- qt --------
  set(PACKAGES_QT ${FS_PACKAGES_DIR}/qt/5.6.2/5.6/gcc_64/lib/cmake/Qt5)
  if(NOT Qt5_DIR AND EXISTS ${PACKAGES_QT})
    set(Qt5_DIR ${PACKAGES_QT})
  endif()
  # set components to search for
  set(QT_COMPONENTS Core Widgets)
  if(NOT APPLE)
    set(QT_COMPONENTS ${QT_COMPONENTS} X11Extras)
  endif()
  # actually find qt
  find_package(Qt5 COMPONENTS ${QT_COMPONENTS})
  if(Qt5_FOUND)
    get_filename_component(QT_INSTALL_DIR "${Qt5_DIR}/../../.." ABSOLUTE)
    if(NOT APPLE)
      symlink(${QT_INSTALL_DIR} ${CMAKE_INSTALL_PREFIX}/lib/qt)
    endif()
  endif()

  # -------- vtk --------
  find_package(VTK)

  # -------- tcltktixblt --------
  find_package(TCLTKTIXBLT)

  # -------- kwwidgets --------
  find_package(KWWidgets)

  # -------- wxwidgets --------
  # only used by mris_decimate_gui
  find_package(WXWidgets)

endif()


# --------------------------------------------------
#             global build configuration
# --------------------------------------------------

add_definitions(-D${CMAKE_SYSTEM_NAME})

# general check for big endian
test_big_endian(IS_BIG_ENDIAN)

# compiler warnings
set(C_CXX_FLAGS "${C_CXX_FLAGS} -Wall -Wno-unused-but-set-variable -Wno-unused-result -Wno-unused-local-typedefs")

# ANSI
add_definitions(-DANSI)

# todo: write some logic to set this correctly
# SSE matrix and math functions (affine.h and sse_mathfun.h)
add_definitions(-DUSE_SSE_MATHFUN)
set(C_CXX_FLAGS "${C_CXX_FLAGS} -msse2 -mfpmath=sse")

# -g compiles debugging info for use by debuggers like gdb. Normally we want
# that in the build, but here we add the FS_DEBUG option, which greatly
# reduces the size of compiled binaries when turned OFF
option(DEBUG "Compile with the -g flag" ON)
if(DEBUG)
  set(C_CXX_FLAGS "${C_CXX_FLAGS} -g")
endif()

# large file support: support for files > 2GB
if(APPLE)
  add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE)
endif()

# optimization
# todo: implement processor-based logic to determine this
set(C_CXX_FLAGS "${C_CXX_FLAGS} -O3 -m64")

# for stripping unused code
if(APPLE)
  set(STRIP_FLAGS "-dead_strip")
else()
  set(STRIP_FLAGS "-fdata-sections -ffunction-sections -Wl,--gc-sections")
  set(C_CXX_FLAGS "${C_CXX_FLAGS} ${STRIP_FLAGS}")
endif()
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${STRIP_FLAGS}")

# link map (only for linux)
if(NOT APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map,ld_map.txt -Wl,--no-demangle")
endif()

# check for 
if(APPLE)
  check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
  if(HAVE_CLOCK_GETTIME)
    add_definitions(-DHAVE_CLOCK_GETTIME)
  endif()
endif()

# apply C and CXX flags
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${C_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_CXX_FLAGS}")


# --------------------------------------------------
#                 build freesurfer
# --------------------------------------------------

# first, prepare the freesurfer distribution
add_subdirectories(distribution images)

# the top-level include dir and the netcdf package contain the most commonly
# included freesurfer header files
set(FS_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include ${MINC_INCLUDE_DIR} ${NetCDF_INCLUDE_DIR})

# now build the freesurfer static libraries (order here matters!)
add_subdirectories(
  unix
  log
  rgb
  hipsstubs
  dicom
  utils
  utilscpp
  fsgdf
  svm-lib
  opencl_algorithms
)

# the following utility libs are required for freeview and qdec
add_subdirectories(vtkfsio vtkutils lineprof)

# the fem elastic subdir also builds a library, so it should be added first
add_subdirectory(fem_elastic)


# --------------------------------------------------
#                      programs
# --------------------------------------------------

# configure the remaining FS subdirs

add_subdirectories(
  anatomicuts
  BrainstemSS
  check_siemens_dir
  connectgraph
  dmri_tensoreig
  dngtester
  freeview
  fsfast
  fslutils
  hiam_make_surfaces
  hiam_make_template
  hiam_register
  HippoSF
  histo_compute_joint_density
  histo_fix_topology
  histo_register
  histo_register_block
  histo_segment
  histo_synthesize
  label2flat
  label2patch
  label_area
  label_border
  lta_convert
  mkxsubjreg
  matlab
  mri_add_xform_to_header
  mri_and
  mri_annotation2label
  mri_aparc2aseg
  mri_aparc2wmseg
  mri_apply_inu_correction
  mri_aseg_edit_reclassify
  mri_aseg_edit_train
  mri_auto_fill
  mri_average
  mri_bc_sc_bias_correct
  mri_bias
  mri_binarize
  mri_build_priors
  mri_ca_label
  mri_ca_normalize
  mri_ca_register
  mri_ca_tissue_parms
  mri_ca_train
  mri_cal_renormalize_gca
  mri_cc
  mri_cht2p
  mri_classify
  mri_cnr
  mri_compile_edits
  mri_compute_change_map
  mri_compute_overlap
  mri_compute_seg_overlap
  mri_compute_structure_transforms
  mri_compute_volume_fractions
  mri_concat
  mri_concatenate_gcam
  mri_concatenate_lta
  mri_convert
  mri_convert_mdh
  mri_copy_values
  mri_cor2label
  mri_coreg
  mri_correct_segmentations
  mri_cvs_register
  mri_dct_align
  mri_deface
  mri_diff
  mri_distance_transform
  mri_divide_segmentation
  mri_edit_segmentation
  mri_edit_segmentation_with_surfaces
  mri_edit_wm_with_aseg
  mri_elastic_energy
  mri_em_register
  mri_estimate_tissue_parms
  mri_evaluate_morph
  mri_extract
  mri_extract_conditions
  mri_extract_fcd_features
  mri_extract_label
  mri_extract_largest_CC
  mri_fcili
  mri_fdr
  mri_fieldsign
  mri_fill
  mri_fit_bias
  mri_fslmat_to_lta
  mri_fuse_intensity_images
  mri_fuse_segmentations
  mri_fwhm
  mri_gca_ambiguous
  mri_gcab_train
  mri_gcut
  mri_gdfglm
  mri_glmfit
  mri_gtmpvc
  mri_gtmseg
  mri_hausdorff_dist
  mri_head
  mri_hires_register
  mri_histo_eq
  mri_histo_normalize
  mri_ibmc
  mri_info
  mri_interpolate
  mri_jacobian
  mri_joint_density
  mri_label2label
  mri_label2vol
  mri_label_accuracy
  mri_label_histo
  mri_label_vals
  mri_label_volume
  mri_linear_register
  mri_log_likelihood
  mri_make_bem_surfaces
  mri_make_density_map
  mri_make_labels
  mri_make_register
  mri_make_template
  mri_map_cpdat
  mri_mark_temporal_lobe
  mri_mask
  mri_matrix_multiply
  mri_mc
  mri_mcsim
  mri_mi
  mri_modify
  mri_morphology
  mri_mosaic
  mri_ms_EM
  mri_ms_EM_with_atlas
  mri_ms_fitparms
  mri_ms_LDA
  mri_multiscale_segment
  mri_multispectral_segment
  mri_nlfilter
  mri_normalize
  mri_normalize_tp2
  mri_otl
  mri_paint
  mri_parse_sdcmdir
  mri_parselabel
  mri_partial_ribbon
  mri_path2label
  mri_polv
  mri_probe_ima
  mri_probedicom
  mri_reduce
  mri_register
  mri_relabel_hypointensities
  mri_relabel_nonwm_hypos
  mri_remove_neck
  mri_rf_label
  mri_rf_long_label
  mri_rf_long_train
  mri_rf_train
  mri_ribbon
  mri_rigid_register
  mri_robust_register
  mri_sbbr
  mri_seg_diff
  mri_segcentroids
  mri_seghead
  mri_segment
  mri_segment_tumor
  mri_segment_wm_damage
  mri_segreg
  mri_segstats
  mri_simulate_atrophy
  mri_stats2seg
  mri_strip_nonwhite
  mri_strip_subject_info
  mri_surf2surf
  mri_surf2vol
  mri_surfacemask
  mri_surfcluster
  mri_synthesize
  mri_tessellate
  mri_threshold
  mri_topologycorrection
  mri_train
  mri_train_autoencoder
  mri_transform
  mri_transform_to_COR
  mri_twoclass
  mri_update_gca
  mri_vol2roi
  mri_vol2surf
  mri_vol2vol
  mri_volcluster
  mri_voldiff
  mri_volsynth
  mri_warp_concat
  mri_warp_convert
  mri_watershed
  mri_wbc
  mri_wmfilter
  mri_xcorr
  mri_xvolavg
  mri_z2p
  mris2rgb
  mris_add_template
  mris_anatomical_stats
  mris_annot_to_segmentation
  mris_apply_reg
  mris_aseg_distance
  mris_average_curvature
  mris_average_parcellation
  mris_BA_segment
  mris_ca_label
  mris_ca_train
  mris_calc
  mris_classify_thickness
  mris_compute_acorr
  mris_compute_lgi
  mris_compute_optimal_kernel
  mris_compute_overlap
  mris_compute_parc_overlap
  mris_compute_volume_fractions
  mris_congeal
  mris_convert
  mris_copy_header
  mris_curvature
  mris_curvature_stats
  mris_decimate
  mris_decimate_gui
  mris_defects_pointset
  mris_deform
  mris_density
  mris_diff
  mris_distance_map
  mris_distance_to_label
  mris_distance_transform
  mris_divide_parcellation
  mris_entropy
  mris_errors
  mris_euler_number
  mris_expand
  mris_extract_patches
  mris_fbirn_annot
  mris_fill
  mris_find_flat_regions
  mris_fix_topology
  mris_flatten
  mris_fwhm
  mris_glm
  mris_hausdorff_dist
  mris_inflate
  mris_info
  mris_init_global_tractography
  mris_interpolate_warp
  mris_jacobian
  mris_label2annot
  mris_label_area
  mris_label_calc
  mris_label_mode
  mris_left_right_register
  mris_longitudinal_surfaces
  mris_make_average_surface
  mris_make_face_parcellation
  mris_make_map_surfaces
  mris_make_surfaces
  mris_make_template
  mris_map_cuts
  mris_merge_parcellations
  mris_mesh_subdivide
  mris_morph_stats
  mris_ms_refine
  mris_ms_surface_CNR
  mris_multiscale_stats
  mris_niters2fwhm
  mris_parcellate_connectivity
  mris_pmake
  mris_register
  mris_register_label_map
  mris_register_to_volume
  mris_remove_variance
  mris_resample
  mris_rescale
  mris_reverse
  mris_rf_label
  mris_rf_train
  mris_rotate
  mris_sample_label
  mris_sample_parc
  mris_seg2annot
  mris_segment
  mris_segment_vals
  mris_segmentation_stats
  mris_shrinkwrap
  mris_simulate_atrophy
  mris_smooth
  mris_sphere
  mris_spherical_average
  mris_surface_change
  mris_surface_stats
  mris_surface_to_vol_distances
  mris_svm_classify
  mris_svm_train
  mris_talairach
  mris_thickness
  mris_thickness_comparison
  mris_thickness_diff
  mris_topo_fixer
  mris_transform
  mris_translate_annotation
  mris_transmantle_dysplasia_paths
  mris_twoclass
  mris_volmask
  mris_volume
  mris_w_to_curv
  mris_warp
  mris_watershed
  mrisp_paint
  mrisp_write
  oct_register_mosaic
  oct_train
  optseq2
  qdecproject
  qdec
  qdec_glmfit
  rbftest
  regdat2xfm
  repair_siemens_file
  scripts
  spherical_stats
  spline3
  stat_normalize
  stim_polar
  swi_processing
  talairach_afd
  talairach_avi
  template
  test_makevol
  ThalamicNuclei
  tkmedit
  tkregister2
  tksurfer
  trc
  tridec
)
