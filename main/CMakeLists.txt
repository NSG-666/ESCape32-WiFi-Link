idf_component_register(
	SRCS main.c oled_ui.c encoder_ui.c wououi.c
	PRIV_REQUIRES
		espressif__mdns
		nixy4__u8g2
		esp-idf-lib__encoder
		driver
		nvs_flash
		esp_wifi
		esp_http_server
		esp_event
		esp_netif
)
set(lang_map en:EN fr:FR de:DE ru:РУ zh:中)
string(REGEX REPLACE "([a-z][a-z]):([^;]+)" "XX(\\1)" LANG_LIST "${lang_map}")
string(REGEX REPLACE "([a-z][a-z]):([^;]+)" "<option value=\"\\1\">\\2</option>" LANG_OPTS "${lang_map}")
list(JOIN LANG_LIST " " LANG_LIST)
list(JOIN LANG_OPTS "" LANG_OPTS)
configure_file(build_defs.h.in build_defs.h @ONLY)
target_include_directories(${COMPONENT_TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
file(GLOB files root*.*)
foreach(file ${files})
	get_filename_component(name ${file} NAME)
	configure_file(${name} ${name} @ONLY)
	add_custom_command(
		OUTPUT ${name}.gz
		COMMAND ${PYTHON} -c "import gzip,shutil,sys; shutil.copyfileobj(open(sys.argv[1],'rb'), gzip.open(sys.argv[1]+'.gz','wb',9))" ${name}
		DEPENDS ${name}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
	target_add_binary_data(${COMPONENT_TARGET} ${CMAKE_CURRENT_BINARY_DIR}/${name}.gz BINARY DEPENDS ${name}.gz)
endforeach()
