################################################################################
# cm5-home (local development source)
################################################################################
CM5_HOME_VERSION = 0.4.0
CM5_HOME_SITE = $(BR2_EXTERNAL_CM5_MEDIA_PATH)/../app
CM5_HOME_SITE_METHOD = local
CM5_HOME_DEPENDENCIES = qt6base qt6declarative qt6multimedia
CM5_HOME_CONF_OPTS = -DCM5_HOME_BUILD_TESTS=OFF
$(eval $(cmake-package))
