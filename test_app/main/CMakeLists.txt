idf_component_register(SRCS "test_app_main.c"
                            "../../components/mgr_ac_logic/test/test_mgr_ac_logic.c"
                        INCLUDE_DIRS ""
                        REQUIRES unity mgr_ac_logic nvs_flash)
