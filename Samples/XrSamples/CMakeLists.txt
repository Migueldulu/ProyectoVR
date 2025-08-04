
# Haciendo este fichero asi podemos hacer el proyecto muy escalable para hacer distintas pruebas

# Esto obtiene una lista con todos los subdirectorios
file(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)

# Loop con la lista de directorios
foreach(child ${children})
    # Si el objeto es un directorio entra
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${child})
        # Add it as a subdirectory
        add_subdirectory(${child})
    endif()
endforeach()
