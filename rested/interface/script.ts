'use strict';

function pushState(state_array: string[])
{
    const uri = (() =>
    {
        const new_search = 'state=' + state_array.join();
        console.log('new search: ', new_search);
        if (window.location.search.length == 0)
        {
            console.log(window.location.href);
            if (window.location.hash.length > 0)
                return window.location.href.replace(window.location.hash, '?'+new_search+window.location.hash);
            else
                return window.location.href + '?' + new_search;
        }
        else
        {
            const search_items = window.location.search.substring(1).split('&');
            let found = false;
            search_items.forEach((item, index) =>
            {
                const kv = item.split('=');
                if (kv[0] == 'state')
                {
                    search_items[index] = new_search;
                    found = true;
                }
            });
            if (!found)
                search_items.push(new_search);
            return window.location.href.replace(window.location.search, '?'+search_items.join('&'));
        }
    })();

	window.history.pushState(null, '', uri);
	root_panel.update(new State(state_array, 0));
}

interface TypeId
{
	type: string;
	id: number;
}

class State
{
	constructor(public readonly array: string[], private readonly index: number) { }

    get next() { return new State(this.array, this.index + 1); }

    get value() { return this.array[this.index]; }
    set value(val: string)
	{
		if (this.array[this.index] != val)
		{
			console.log('state change: ', this.array[this.index], ' -> ', val);
			this.array[this.index] = val;
			pushState(this.array);
		}
	}

	get typeId(): TypeId
	{
		const v = this.value;
		if (v === undefined)
			return undefined;
		const digit_start = v.search(/\d/);
		return { type: v.substring(0, digit_start), id: +v.substring(digit_start) };
	}

	replace(...values: string[]) : void
	{
		this.array.length = this.index;
		values.forEach(value => this.array.push(value));
		pushState(this.array);
	}
}

class Tab
{
	constructor(public readonly name: string, private make_content: () => HTMLElement)
    {
	}
	get content() { return this.make_content(); }
}


//function get_json(response: any) { return response.text().then((text: string) => (console.log('recieved:', text), JSON.parse(text))); }
function get_json(response: any) { return response.json(); }
function make_element(type: string, text?: string)
{
    const e = document.createElement(type);
    text && (e.textContent = text);
    return e;
}
function make_div(classname: string)
{
    const div = document.createElement('div');
    if (classname !== undefined)
        div.classList.add(classname);
    return div;
}

function select(element: Element)
{
    for (let s = element.previousElementSibling; s != null; s = s.previousElementSibling)
        s.classList.remove('selected');
    for (let s = element.nextElementSibling; s != null; s = s.nextElementSibling)
        s.classList.remove('selected');
    element.classList.add('selected');
}


class TabView
{
	public element: HTMLElement;
	private active_tab: number;

	constructor(public state: State, private tabs: Tab[])
	{
		this.active_tab = +(state.value || 0);

        let bar = make_div('tab-bar');
        tabs.forEach((t, i) =>
        {
            const tab = make_div('tab');
            tab.textContent = t.name;
            tab.onclick = () => 
            {
				this.state.value = "" + i;
            };
            bar.appendChild(tab);
        });
		bar.children[this.active_tab].classList.add('selected');
		let content = tabs[this.active_tab].content;

		this.element = make_div('tab-view');
		this.element.appendChild(bar);
		this.element.appendChild(content);
	}

    update(state: State) : State
	{
		const new_tab = +(state.value || 0);
		this.state = state;
		if (this.active_tab != new_tab)
		{
			console.log('New tab:', state.value, state.array);
			this.active_tab = new_tab;
			select(this.element.firstElementChild.children[this.active_tab]);
			this.element.replaceChild(this.tabs[this.active_tab].content, this.element.lastElementChild);
		}
		return state.next;
    }
}

function makeItem(name: string, on_select: () => void)
{
    const item = make_element('li', name);
	item.onclick = () =>
	{
        select(item);
        on_select();
    };
    return item;
}

function getCharacter(character_id: number, new_panel: any)
{
	console.log("fetch character " + character_id);
	new_panel((panel: HTMLElement, set_child: any) =>
    {
        fetch('/characters/' + character_id).then(get_json)
            .then(c => {
                if (c.length != 1) {
                    console.log('character ' + character_id + ' has unexpectedly dissapeared');
                    return;
                }
                c = c[0];
                console.log('got character ' + c.id);

                panel.appendChild(make_element('h3', c.name));

                var form = document.createElement('form');

                var stats = ['str', 'dex', 'nte', 'emp', 'ntu'];
				const stat_names: { [key: string]: string } =
                {
                    'str': 'Strength',
                    'dex': 'Dexterity',
                    'nte': 'Intelligence',
                    'emp': 'Empathy',
                    'ntu': 'Intuition'
                };
                stats.forEach(function (stat) {
                    var row = document.createElement('div');
                    row.classList.add('stat');
                    var label = document.createElement('div'); label.textContent = stat_names[stat];
                    var value = document.createElement('div'); value.textContent = c[stat];
                    row.appendChild(label);
                    row.appendChild(value);
                    form.appendChild(row);
                });

                panel.appendChild(form);
            });
    });
    //updateState('char', character_id);
}

interface NameId
{
	id: number;
	name: string;
}

function makeCharacterItems(list: HTMLElement, data: NameId[], state: State)
{
	console.log('got ' + data.length + ' characters');
	if (data.length == 0)
		list.appendChild(make_element('p', 'No characters'));
	else
		data.forEach(c =>
		{
			list.appendChild(makeItem(c.name, () => state.replace('character' + c.id)));
		});
}

function getAllCharacters(state: State)
{
	const list = make_element('ul');
	console.log('fetch characters');
	fetch('/characters/id,name').then(get_json)
		.then((data: NameId[]) => makeCharacterItems(list, data, state))
		.catch(error => console.log(error));
	return list;

}

function getCharactersAt(place_id: number, state: State)
{
    const list = make_element('ul');
	console.log('fetch characters');
	fetch('/characters/id,name?place=' + (place_id > 0 ? '' + place_id : 'null')).then(get_json)
		.then((data: NameId[]) => makeCharacterItems(list, data, state))
        .catch(function (error) {
            console.log(error);
        });
    return list;
}

function getPlaces(state: State)
{
    const list = make_element('ul');
    console.log('fetch places');
    fetch('/places/id,name').then(get_json)
    .then((data: NameId[]) =>
    {
		console.log('got ' + data.length + ' places');
		list.appendChild(makeItem('No place', () => state.replace('place0')));
		data.forEach(p =>
		{
			list.appendChild(makeItem(p.name, () => state.replace('place'+p.id)));
        });
    })
    .catch(function (error) {
        console.log(error);
    });
    return list;
}

function stateFromLocation()
{
    return new State(
            (window.location.search.substring(1).split('&')
            .find(item => item.startsWith('state=')) || 'state=0').split('=')[1].split(','), 
            0);
}

interface PanelElement
{
	update: (state: State) => State;
	element: HTMLElement;
}

class Panel
{
	public readonly element: HTMLElement;
	private children: PanelElement[] = [];
	private next: Panel = null;
	public static generator: { [key: string]: (id: number, state: State) => Panel } = { };

	constructor(public type: string, header: string)
    {
        this.element = make_div('panel');
        this.element.appendChild(make_element('h3', header));

        document.getElementById('panels').appendChild(this.element);
	}

	append(child: PanelElement): void
	{
		this.children.push(child);
		this.element.appendChild(child.element);
	}



	updateNext(state: State)
	{
		const panels = this.element.parentElement;
		const clear_next = () => 
		{
			const parent = this.element.parentElement;
			while (parent.lastChild != this.element)
				parent.removeChild(parent.lastChild);
			this.next = null;
		}
		const adjust_transform = () =>
		{
			const w_width = window.innerWidth;
			const p_width = panels.clientWidth;
			panels.style.transform = 'translateX(' + Math.min(0, w_width - p_width) + 'px)';
		}
		if (state.value === undefined)
		{
			clear_next();
			adjust_transform();
			return;
		}
		const { type, id } = state.typeId;
		console.log('updateNext(' + type + '/' + id + ')');
		console.log(' type:', (this.next ? this.next.type : null));
		if ((this.next ? this.next.type : null) != type)
		{
			clear_next();
			if (type !== undefined)
			{
				console.log('generate: type:', type, 'id:', id);
				this.next = Panel.generator[type](id, state.next);
				panels.appendChild(this.next.element);
			}
		}
		else if (this.next)
		{
			this.next.update(state.next);
		}
		adjust_transform();
	}

	update(state: State)
	{
		this.children.forEach((child) => state = child.update(state));
		this.updateNext(state);
	}
}


Panel.generator.root = (id, state) => 
{
	const panel = new Panel('root', 'Rolle');
    const tabs = new TabView(state,
		[
			new Tab("Characters", () => getAllCharacters(state.next)),
			new Tab("Places", () => getPlaces(state.next)),
			new Tab("Groups", () => make_element('p', 'Groups are listed here'))
		]);
	panel.element.appendChild(tabs.element);
	panel.append(tabs);
	panel.updateNext(state.next);
    return panel;
}

Panel.generator.place = (id, state) =>
{
	if (id < 0)
		throw new RangeError('Place id invalid');
	const panel = new Panel('place', 'Fetching place');
	const build_panel = (place: any) =>
	{
		const tabs = new TabView(state,
			[
				new Tab("Description", () => make_element('p', 'This place has no description')),
				new Tab("Characters", () => getCharactersAt(id, state.next))
			]);
		panel.element.firstElementChild.textContent = place.name;
		panel.append(tabs);
	};

	if (id == 0)
		build_panel({ name: "No place" });
	else
	{
		console.log('fetch place ' + id);
		fetch('/places/' + id).then(get_json).then((places) => build_panel(places[0]));
	}

	panel.updateNext(state.next.next);
	return panel;
}

interface CharacterData
{
	[key: string]: any;
	id: number;
	name: string;
}

Panel.generator.character = (id, state) =>
{
	const panel = new Panel('character', 'Fetching character');
	console.log("fetch character " + id);
	fetch('/characters/' + id).then(get_json)
		.then((data: CharacterData[]) =>
		{
			if (data.length != 1)
			{
				console.log('character ' + id + ' has unexpectedly dissapeared');
				return;
			}
			const c = data[0];
			console.log('got character ' + c.id);

			panel.element.firstChild.textContent = c.name;

			const form = make_element('form');

			const stats = ['str', 'dex', 'nte', 'emp', 'ntu'];
			const stat_names: { [key: string]: string } =
				{
					'str': 'Strength',
					'dex': 'Dexterity',
					'nte': 'Intelligence',
					'emp': 'Empathy',
					'ntu': 'Intuition'
				};
			stats.forEach(stat =>
			{
				var row = make_element('div');
				row.classList.add('stat');
				row.appendChild(make_element('div', stat_names[stat]));
				row.appendChild(make_element('div', c[stat]));
				form.appendChild(row);
			});

			panel.element.appendChild(form);
		});
	return panel;
}

function initialize()
{
    const state = stateFromLocation();
    console.log('Initial state: ', state.array);
    return Panel.generator.root(0, state);
}
var root_panel = initialize();

window.onpopstate = () => root_panel.update(stateFromLocation());
