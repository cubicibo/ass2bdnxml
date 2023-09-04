project('ass2bdnxml', 'c')

src = ['ass2bdnxml.c', 'render.c']

deps = [
    dependency('libass', required: true),
    dependency('libpng', required: true),
    dependency('imagequant', required: true),
    meson.get_compiler('c').find_library('m', required: false)
]

executable(meson.project_name(), src, dependencies: deps)
