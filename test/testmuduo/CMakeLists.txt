

#添加头文件搜索路径
# include_directories()
#添加库文件搜索路径
# link_directories()

#设置需要编译的源文件列表
set(SRC_LIST ./muduo_server.cpp)

#设置可执行文件最终存储的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)


#把.指定路径下所有的源文件加入到SRC_LIST中
# aux_source_directory(. SRC_LIST)

#生成可执行文件server
add_executable(server ${SRC_LIST})

#server需要链接三个库文件
target_link_libraries(server muduo_net muduo_base pthread)
