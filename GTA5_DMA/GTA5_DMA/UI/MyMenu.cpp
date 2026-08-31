#include "pch.h"

#include "MyMenu.h"
#include "MenuManager.h"

bool MyMenu::Render()
{
    MenuManager::GetInstance().RenderCurrentPage();
    return true;
}
