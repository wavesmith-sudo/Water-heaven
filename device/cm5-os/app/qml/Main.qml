import QtQuick
import QtQuick.Controls
import QtMultimedia
Window {
 id: root; visible: true; visibility: Window.FullScreen
 title: "Retro Arcade Media OS"; color: "#100c20"
 property string page: "Home"
 property int selected: 0
 property string selectedItem: ""
 property var categories: ["Movies","TV Shows","Music","Retro Games","Settings","Continue Watching"]
 property string accent: "#29e8f2"
 function openCategory(name) {
   if(name === "Retro Games") { page="Games"; selected=0 }
   else if(name === "Settings") { page="Settings"; selected=0 }
   else { page="Library"; selected=0; jellyfin.browse(name) }
   currentCategory=name
 }
 property string currentCategory: "Movies"
 function back() {
   if(page==="Player") { player.stop(); page="Library" }
   else {page="Home";selected=0}
 }
 Connections { target: jellyfin; function onLoggedIn() { root.page="Home" } }
 Rectangle {anchors.fill:parent; color:"#100c20"}
 Column {
   anchors.fill:parent; anchors.margins:Math.max(30,root.width*.055); spacing:22
   Row {spacing:20
     Text {text:"RETRO ARCADE";font.pixelSize:30;font.bold:true;color:"#e95bff"}
     Text {text:"  /  "+root.page; font.pixelSize:26;color:"#c0b5db"}
   }
   Text {visible:page==="Home";text:"STREAM  •  PLAY  •  REPEAT";color:accent;font.pixelSize:34;font.bold:true}
   Grid {
     visible:page==="Home";columns:3;spacing:20
     Repeater {model:root.categories
       Rectangle {required property int index;required property string modelData
         width:Math.min(340,(root.width-140)/3);height:170;radius:12
         color:index===root.selected?"#302351":"#211932"
         border.color:index===root.selected?accent:"#53416d";border.width:index===root.selected?4:1
         Text {anchors.centerIn:parent;text:modelData;color:"white";font.pixelSize:27;font.bold:true}
       }
     }
   }
   Text {visible:page==="Library";text:currentCategory.toUpperCase();font.pixelSize:32;color:accent}
   GridView {
     id: library; visible:page==="Library";width:parent.width;height:parent.height-150
     cellWidth:240;cellHeight:300;clip:true;model:jellyfin.items
     delegate:Rectangle {
       required property int index; required property var modelData
       width:220;height:280;radius:10;color:index===root.selected?"#392e59":"#1e1930"
       border.color:index===root.selected?accent:"#504563";border.width:index===root.selected?3:1
       Column {anchors.fill:parent;anchors.margins:9;spacing:10
         Image {source:modelData.image; width:200;height:230;fillMode:Image.PreserveAspectFit;asynchronous:true}
         Text {text:modelData.name;elide:Text.ElideRight;width:200;color:"white";font.pixelSize:16}
       }
     }
   }
   Column {visible:page==="Settings";spacing:12
     Text {text:"JELLYFIN CONNECTION";color:accent;font.pixelSize:27}
     TextField {id:server; width:620;text:jellyfin.server;placeholderText:"https://media.example.com"}
     TextField {id:username;width:620;placeholderText:"Username"}
     TextField {id:password;width:620;placeholderText:"Password";echoMode:TextInput.Password}
     Button {text:"Connect";onClicked:{jellyfin.server=server.text;jellyfin.login(username.text,password.text)}}
     Text {text:jellyfin.error;color:"#ff8ab8";font.pixelSize:18}
     Text {text:"Use HTTPS outside a trusted local network";color:"#c6bed7"}
   }
   Text {visible:page==="Games";text:"RetroArch integration is a future milestone";color:"white";font.pixelSize:24}
 }
 VideoOutput {id:video;visible:page==="Player";anchors.fill:parent;fillMode:VideoOutput.PreserveAspectFit}
 MediaPlayer {id:player;videoOutput:video;audioOutput:AudioOutput {}}
 Text {visible:page==="Player";anchors.bottom:parent.bottom;anchors.horizontalCenter:parent.horizontalCenter
       text:"OK: Pause/Play    Back: Return";color:"white";font.pixelSize:22}
 Text {visible:jellyfin.busy;anchors.right:parent.right;anchors.top:parent.top;text:"Loading…";color:accent;font.pixelSize:20}
 Item {anchors.fill:parent;focus:true
   Keys.onPressed:function(e) {
     if(e.key===Qt.Key_Escape||e.key===Qt.Key_Backspace||e.key===Qt.Key_Back){root.back();e.accepted=true;return}
     if(root.page==="Settings")return
     if(root.page==="Player"){
       if(e.key===Qt.Key_Return||e.key===Qt.Key_Enter||e.key===Qt.Key_Space){player.playbackState===MediaPlayer.PlayingState?player.pause():player.play();e.accepted=true}
       return
     }
     let count=root.page==="Home"?root.categories.length:(root.page==="Library"?jellyfin.items.length:0)
     let columns=root.page==="Home"?3:Math.max(1,Math.floor(library.width/library.cellWidth))
     if(e.key===Qt.Key_Right)root.selected=Math.min(count-1,root.selected+1)
     else if(e.key===Qt.Key_Left)root.selected=Math.max(0,root.selected-1)
     else if(e.key===Qt.Key_Down)root.selected=Math.min(count-1,root.selected+columns)
     else if(e.key===Qt.Key_Up)root.selected=Math.max(0,root.selected-columns)
     else if(e.key===Qt.Key_Return||e.key===Qt.Key_Enter||e.key===Qt.Key_Select){
       if(root.page==="Home"){
         let c=root.categories[root.selected]
         if(c==="Continue Watching"){root.page="Library";root.currentCategory=c;root.selected=0;jellyfin.continueWatching()}
         else root.openCategory(c)
       } else if(root.page==="Library" && count>0){
         let item=jellyfin.items[root.selected]
         if(item.type==="Movie"||item.type==="Episode"||item.type==="Video"||item.type==="Audio"){
           player.source=jellyfin.playbackUrl(item.id);root.page="Player";player.play()
         } else if(item.type==="Series") {root.selectedItem=item.id;root.page="Series"}
       }
     } else return
     e.accepted=true
   }
 }
 Text {visible:page==="Series";anchors.centerIn:parent;text:"Episode browsing pending: "+selectedItem;color:"white";font.pixelSize:24}
}
