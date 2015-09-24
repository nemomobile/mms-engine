TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += \
  mms-lib \
  mms-handler-dbus \
  mms-ofono \
  mms-settings-dconf \
  mms-engine \
  mms-dump \
  mms-send
OTHER_FILES += \
  rpm/mms-engine.spec \
  README
