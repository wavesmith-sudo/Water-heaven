################################################################################
# cm5-home (local development source)
################################################################################
CM5_HOME_VERSION = 1.0
CM5_HOME_SITE = $(BR2_EXTERNAL_CM5_MEDIA_PATH)/../app
CM5_HOME_SITE_METHOD = local
CM5_HOME_DEPENDENCIES = qt6base qt6declarative qt6multimedia
$(eval $(cmake-package))
