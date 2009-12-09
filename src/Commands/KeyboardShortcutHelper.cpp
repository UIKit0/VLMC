#include <QtDebug>

#include "SettingValue.h"
#include "SettingsManager.h"
#include "KeyboardShortcutHelper.h"

KeyboardShortcutHelper::KeyboardShortcutHelper( const QString& name, QWidget* parent ) :
        QShortcut( parent ),
        m_name( name )
{
    const SettingValue*   set = SettingsManager::getInstance()->getValue( "keyboard_shortcut", name );
    qDebug() << set->get().toString();
    setKey( QKeySequence( set->get().toString() ) );
    connect( set, SIGNAL( changed( const QVariant& ) ), this, SLOT( shortcutUpdated( const QVariant& ) ) );
}

void    KeyboardShortcutHelper::shortcutUpdated( const QVariant& value )
{
    qDebug() << "Key sequence updated:" << value.toString();
    setKey( QKeySequence( value.toString() ) );
}
