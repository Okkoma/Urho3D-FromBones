//
// Copyright (c) 2008-2016 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Graphics/Graphics.h"
#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/XMLFile.h"
#include "../Input/Input.h"
#include "../UI/Button.h"
#include "../UI/MessageBox.h"
#include "../UI/Text.h"
#include "../UI/UI.h"
#include "../UI/UIEvents.h"
#include "../UI/Window.h"

namespace Urho3D
{

MessageBox::MessageBox(Context* context, const String& messageString, const String& titleString, XMLFile* layoutFile,
    XMLFile* styleFile) :
    Object(context),
    titleText_(0),
    messageText_(0),
    okButton_(0),
    keepAliveOnEscape_(false)
{
//    URHO3D_LOGINFO("MessageBox() !");

    Graphics* graphics = GetSubsystem<Graphics>();  // May be null if headless
    UI* ui = GetSubsystem<UI>();
	if (!ui || !graphics)
    {
        URHO3D_LOGWARNING("MessageBox() - Instantiating a modal window in headless mode or no ui !");
        return;
    }

    // If layout file is not given, use the default message box layout
    if (!layoutFile)
    {
        ResourceCache* cache = GetSubsystem<ResourceCache>();
        layoutFile = cache->GetResource<XMLFile>("UI/MessageBox.xml");
        if (!layoutFile)    // Error is already logged
            return;         // Note: windowless MessageBox should not be used!
    }

    window_ = ui->LoadLayout(layoutFile, styleFile);
    if (!window_)   // Error is already logged
        return;
    ui->GetRoot()->AddChild(window_);

    // Set the title and message strings if they are given
    titleText_ = window_->GetChildDynamicCast<Text>("TitleText", true);
    if (titleText_ && !titleString.Empty())
        titleText_->SetText(titleString);
    messageText_ = window_->GetChildDynamicCast<Text>("MessageText", true);
    if (messageText_ && !messageString.Empty())
        messageText_->SetText(messageString);

    // Center window after the message is set
    Window* window = dynamic_cast<Window*>(window_.Get());
    if (window)
    {
        const IntVector2& size = window->GetSize();
//        window->SetPosition((graphics->GetWidth() - size.x_) / 2 / ui->GetScale(), (graphics->GetHeight() - size.y_) / 2 / ui->GetScale());
        window->SetPosition((ui->GetRoot()->GetSize() - size) / 2);
        window->SetModal(true);
        SubscribeToEvent(window, E_MODALCHANGED, URHO3D_HANDLER(MessageBox, HandleMessageAcknowledged));
    }

    // Bind the buttons (if any in the loaded UI layout) to event handlers
    okButton_ = window_->GetChildDynamicCast<Button>("OkButton", true);
    if (okButton_)
    {
        ui->SetFocusElement(okButton_);
        SubscribeToEvent(okButton_, E_RELEASED, URHO3D_HANDLER(MessageBox, HandleMessageAcknowledged));
    }
    Button* cancelButton = window_->GetChildDynamicCast<Button>("CancelButton", true);
    if (cancelButton)
        SubscribeToEvent(cancelButton, E_RELEASED, URHO3D_HANDLER(MessageBox, HandleMessageAcknowledged));
    Button* closeButton = window_->GetChildDynamicCast<Button>("CloseButton", true);
    if (closeButton)
        SubscribeToEvent(closeButton, E_RELEASED, URHO3D_HANDLER(MessageBox, HandleMessageAcknowledged));

    // Increase reference count to keep Self alive
//    AddRef();
}

MessageBox::~MessageBox()
{
//    URHO3D_LOGINFO("~MessageBox()");
    // This would remove the UI-element regardless of whether it is parented to UI's root or UI's modal-root
    if (window_)
        window_->Remove();
}

void MessageBox::RegisterObject(Context* context)
{
    context->RegisterFactory<MessageBox>();
}

void MessageBox::SetTitle(const String& text)
{
    if (titleText_)
        titleText_->SetText(text);
}

void MessageBox::SetMessage(const String& text)
{
    if (messageText_)
        messageText_->SetText(text);
}

void MessageBox::SetAliveOnEscapeKey(bool enable)
{
    keepAliveOnEscape_ = enable;
}

const String& MessageBox::GetTitle() const
{
    return titleText_ ? titleText_->GetText() : String::EMPTY;
}

const String& MessageBox::GetMessage() const
{
    return messageText_ ? messageText_->GetText() : String::EMPTY;
}

void MessageBox::HandleMessageAcknowledged(StringHash eventType, VariantMap& eventData)
{
    using namespace MessageACK;

    if (keepAliveOnEscape_)
    {
        if (context_->GetSubsystem<Input>()->GetKeyPress(KEY_ESCAPE))
            return;
    }

    VariantMap& newEventData = GetEventDataMap();
    newEventData[P_OK] = eventData[Released::P_ELEMENT] == okButton_;
    SendEvent(E_MESSAGEACK, newEventData);

    // Self destruct
    this->~MessageBox();
}

}
