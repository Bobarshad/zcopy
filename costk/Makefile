include $(TOPDIR)/rules.mk

PKG_NAME:=costk
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=Base
  CATEGORY:=Network Support
  TITLE:=Varnet costk network service
  DEPENDS := +libstdcpp +librt +libpthread
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(TARGET_CONFIGURE_OPTS)
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/bin $(1)/etc/init.d
	$(INSTALL_BIN) files/costk.init  $(1)/etc/init.d/costk
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cstk $(1)/bin/
endef

$(eval $(call BuildPackage,costk))
