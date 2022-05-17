/*
author: fulavio
*/
#pragma once

#include "ui/layers/generic_box.h"
#include "boxes/abstract_box.h"

namespace Data {
class Session;
class Changes;
} // namespace Data

namespace Main {
class SessionSettings;
} //namespace Main

namespace Window {
class SessionController;
} // namespace Window

void EditKeywordsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		not_null<DocumentData*> document);
