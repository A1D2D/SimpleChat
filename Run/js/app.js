let selectedId = "0";
let selectedClientID = -1;
let contextInstance = null;

function createCategory(category) {
   const categoryElement = document.createElement("div");
   categoryElement.className = "list-category";
   categoryElement.dataset.type = category.type;

   const header = document.createElement("div");
   header.className = "list-header";

   const arrow = document.createElement("span");
   arrow.className = "arrow";
   arrow.textContent = "▶";

   const name = document.createElement("span");
   name.textContent = category.type;

   header.appendChild(arrow);
   header.appendChild(name);

   const items = document.createElement("div");
   items.className = "list-items";

   header.addEventListener("click", () => {
      const open = items.classList.toggle("open");
      arrow.textContent = open ? "▼" : "▶";
   });

   for (const instance of category.instances) {
      items.appendChild(createInstance(instance));
   }

   categoryElement.appendChild(header);
   categoryElement.appendChild(items);

   return categoryElement;
}

function createInstance(instance) {
   const instElement = document.createElement("div");
   instElement.className = "list-item";

   let hasConnections = false;
   const items = document.createElement("div");
   items.className = "list-connections";
   let connectionIndex = 0;
   for (const connection of instance.clients) {
      hasConnections = true;
      items.appendChild(createConnection(connection, instance, connectionIndex++));
   }

   const header = document.createElement("div");
   header.className = "list-header";

   if(hasConnections) {
      const arrow = document.createElement("span");
      arrow.className = "arrow";
      arrow.textContent = "▶";
      header.appendChild(arrow);

      arrow.addEventListener("click", () => {
      const open = items.classList.toggle("open");
      arrow.textContent = open ? "▼" : "▶";
   });
   }

   const name = document.createElement("span");
   name.textContent = instance.id;
   name.id = "name";

   name.addEventListener("click", event => {
      event.stopPropagation();

      document
         .querySelectorAll("#name.selected")
         .forEach(x => x.classList.remove("selected"));

      name.classList.add("selected");

      selectInstance(instance.id, -1);
   });

   name.addEventListener("contextmenu", event => {
      event.preventDefault();
      event.stopPropagation();

      selectInstance(instance.id, -1);

      log("Right clicked:", instance.id);

      showInstanceMenu(
         event.clientX,
         event.clientY,
         instance.id
      );
   });

   header.appendChild(name);

   instElement.appendChild(header);
   if(hasConnections) instElement.appendChild(items);

   return instElement;
}

function createConnection(connection, instance, index) {
   const element = document.createElement("div");
   element.id = "name";

   element.className = "connection";
   element.textContent = connection.id;

   element.addEventListener("click", event => {
      event.stopPropagation();

      document
         .querySelectorAll("#name.selected")
         .forEach(x => x.classList.remove("selected"));

      element.classList.add("selected");

      selectInstance(instance.id, index);
   });

   element.addEventListener("contextmenu", event => {
      event.preventDefault();
      event.stopPropagation();

      selectInstance(instance.id, index);

      log("Right clicked:", instance.id);

      showInstanceMenu(
         event.clientX,
         event.clientY,
         instance.id
      );
   });

   return element;
}

function updateInstances(data) {
   const root = document.getElementById("instance-list");

   const openCategories = new Set();

   root.querySelectorAll(".list-category").forEach(category => {
      const type = category.dataset.type;
      const items = category.querySelector(".list-items");

      if (items.classList.contains("open")) openCategories.add(type);
   });

   root.innerHTML = "";

   for (const category of data) {
      const element = createCategory(category);

      if (openCategories.has(category.type)) {
         const items = element.querySelector(".list-items");
         const arrow = element.querySelector(".arrow");

         items.classList.add("open");
         arrow.textContent = "▼";
      }

      root.appendChild(element);
   }
}

function showInstanceMenu(x, y, id) {
   const menu = document.getElementById("instance-menu");

   contextInstance = id;

   menu.style.left = `${x}px`;
   menu.style.top = `${y}px`;
   menu.style.display = "block";

   log("Context menu for:", id);
   log("Position:", x, y);
}

document.addEventListener("click", () => {
   document.getElementById("instance-menu").style.display = "none";
});

document.getElementById("menu-remove").addEventListener("click", () => {
   log("Remove:", contextInstance);
   remove(selectedId);
   refreshInstances();

   document.getElementById("instance-menu").style.display = "none";
});

//Resolve
document.getElementById("resolve-button").addEventListener("click", () => {
   openResolveMenu();
});

function openResolveMenu() {
   document.getElementById("instance-menu").style.display = "none";

   document.getElementById("resolve-modal").style.display = "flex";
   document.getElementById("resolve-ip").focus();
}

document.getElementById("resolve-cancel").addEventListener("click", () => {
   document.getElementById("resolve-modal").style.display = "none";
});

document.getElementById("resolve-confirm").addEventListener("click", () => {
   const protocol = document.getElementById("resolve-protocol").value;
   const ipInput = document.getElementById("resolve-ip");
   const portInput = document.getElementById("resolve-port");

   const ip = ipInput.value || ipInput.placeholder;
   const port = Number(portInput.value || portInput.placeholder);

   log("resolve:", protocol, ip, port);
   resolve(protocol, ip, port);
   refreshInstances();

   document.getElementById("resolve-modal").style.display = "none";
});


//Connect
document.getElementById("menu-connect").addEventListener("click", () => {
   openConnectMenu();
});

document.getElementById("connect-button").addEventListener("click", () => {
   openConnectMenu();
});

function openConnectMenu() {
   document.getElementById("instance-menu").style.display = "none";

   document.getElementById("connect-modal").style.display = "flex";
   document.getElementById("connect-ip").focus();
}

document.getElementById("connect-cancel").addEventListener("click", () => {
   document.getElementById("connect-modal").style.display = "none";
});

document.getElementById("connect-confirm").addEventListener("click", () => {
   const protocol = document.getElementById("connect-protocol").value;
   const ipInput = document.getElementById("connect-ip");
   const portInput = document.getElementById("connect-port");

   const ip = ipInput.value || ipInput.placeholder;
   const port = Number(portInput.value || portInput.placeholder);

   log("Connect:", protocol, ip, port);
   connect(protocol, ip, port);
   refreshInstances();

   document.getElementById("connect-modal").style.display = "none";
});


//Start
document.getElementById("menu-start").addEventListener("click", () => {
   openStartServer();
});

document.getElementById("start-button").addEventListener("click", () => {
   openStartServer();
});

function openStartServer() {
   document.getElementById("instance-menu").style.display = "none";

   document.getElementById("start-modal").style.display = "flex";
   document.getElementById("start-port").focus();
}

document.getElementById("start-cancel").addEventListener("click", () => {
   document.getElementById("start-modal").style.display = "none";
});

document.getElementById("start-confirm").addEventListener("click", () => {
   const protocol = document.getElementById("start-protocol").value;
   const sPortElement = document.getElementById("start-port");
   const port = Number(sPortElement.value || sPortElement.placeholder);

   log("start:", protocol, port);
   start(protocol, port);
   refreshInstances();

   document.getElementById("start-modal").style.display = "none";
});


//Stop
document.getElementById("stop-button").addEventListener("click", () => {
   stopInstance();
});


//Disconnect
document.getElementById("disconnect-button").addEventListener("click", () => {
   disconnect(selectedClientID);
});

//Chat
function clearChat() {
   const output = document.getElementById("chat-output");
   output.innerHTML = "";
}

function chatWrite(message, type = "normal") {
   const output = document.getElementById("chat-output");

   const line = document.createElement("div");
   line.className = "chat-line " + type;
   line.textContent = message;

   output.appendChild(line);
   output.scrollTop = output.scrollHeight;
}

const chatInput = document.getElementById("chat-msg");

chatInput.addEventListener("keydown", function(event) {
   if (event.key !== "Enter") return;

   if (event.shiftKey) {
      return;
   }

   event.preventDefault();

   const message = chatInput.value.trim();

   if (message === "") return;
   sendMessage(selectedClientID, message);

   chatInput.value = "";
});


//Console
function consoleWrite(message, type = "normal") {
   const output = document.getElementById("console-output");

   const line = document.createElement("div");
   line.className = "console-line " + type;
   line.textContent = message;

   output.appendChild(line);
   output.scrollTop = output.scrollHeight;
}

const consoleInput = document.getElementById("console-command");

consoleInput.addEventListener("keydown", function(event) {
   if (event.key !== "Enter") return;

   const command = consoleInput.value.trim();

   if (command === "") return;

   consoleWrite("> " + command, "normal");
   sendCommand(command);

   consoleInput.value = "";
});


function selectInstance(id, clinetIndex) {
   selectedId = id;
   selectedClientID = clinetIndex;
   if(clinetIndex == -1) {
      log("Selected:", id);
   } else {
      log("Selected:", id, clinetIndex);
   }
   use(id);
   refreshChat();
}

async function refreshInstances() {
   const data = await window.getInstances("");
   updateInstances(data);
   refreshChat();
}

function addInstance() {
   const instanceID = document.getElementById("instanceID").value;
   if (instanceID == "") return;
   log(`added instance ${instanceID}`);
   use(instanceID);

   refreshInstances();
};

document.getElementById("add").addEventListener("click", addInstance);

document.getElementById("instanceID").addEventListener("keydown", event => {
   if (event.key === "Enter") {
      event.preventDefault();
      addInstance();
   }
});

document.getElementById("remove").addEventListener("click", () => {
   remove(selectedId);
   refreshInstances();
});

// document.getElementById("test").addEventListener("click", () => {
//    log("button pressed");
//    consoleWrite("button pressed");
// });

refreshInstances();

// Info 0
// Warning 1
// Server 2
// Client 3
// Connection 4
function updateChat(data) {
   clearChat();
   for (const message of data) {
      switch (message.type) {
         case 0:
            chatWrite(message.msg, "info");
            break;
         case 1:
            chatWrite(message.msg, "error");
            break;
         case 2:
            chatWrite(`Server: ${message.msg}`);
            break;
         case 3:
            chatWrite(`Client: ${message.msg}`);
            break;
         case 4:
            chatWrite(`Connection: ${message.id}: ${message.msg}`);
            break;
      
         default:
            break;
      }
   }
}

async function refreshChat() {
   const data = await window.getMessages("");
   updateChat(data);
}

//c++ callable
function consoleError(message) {
   refreshInstances();
   refreshChat();
   consoleWrite(message, "error");
}

function consoleInfo(message) {
   refreshInstances();
   refreshChat();
   consoleWrite(message, "info");
}