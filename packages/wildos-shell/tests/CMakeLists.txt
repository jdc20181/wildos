find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

add_executable(underglowtest underglowtest.cpp ../src/underglowsettings.cpp)
target_include_directories(underglowtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(underglowtest PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME underglow COMMAND underglowtest)
