open Util.ReactStuff;

%raw
"require('../styles/main.css')";

/*
    TODO / Idea list:
    - Convert language dropdown to toggle component
    - Allow syntax switching on errornous syntax
    - Add settings pane to set moduleSystem
    - Add advanced mode for enabling OCaml output as well

    More advanced tasks:
    - Fix syntax convertion issue where comments are stripped on Reason <-> Res convertion
 */

open CompilerManagerHook;
module Api = Bs_platform_api;

module DropdownSelect = {
  type style = [ | `Error | `Normal];

  [@react.component]
  let make = (~onChange, ~name, ~value, ~disabled=false, ~children) => {
    let opacity = disabled ? " opacity-50" : "";
    <select
      className={
        "border border-night-light inline-block rounded px-4 py-1 bg-onyx appearance-none font-semibold "
        ++ opacity
      }
      name
      value
      disabled
      onChange>
      children
    </select>;
  };
};

module LanguageToggle = {
  [@react.component]
  let make =
      (
        ~onChange: Api.Lang.t => unit,
        ~values: array(Api.Lang.t),
        ~selected: Api.Lang.t,
        ~disabled=false,
      ) => {
    // We make sure that there's at least one element in the array
    // otherwise we run into undefined behavior
    let values =
      if (Array.length(values) === 0) {
        [|selected|];
      } else {
        values;
      };

    let selectedIndex =
      switch (Belt.Array.getIndexBy(values, lang => {lang === selected})) {
      | Some(i) => i
      | None => 0
      };

    let elements =
      Belt.Array.mapWithIndex(
        values,
        (i, lang) => {
          let active = i === selectedIndex ? "text-fire" : "";
          let ext = Api.Lang.toExt(lang)->Js.String2.toUpperCase;

          <span key=ext className={"mr-1 last:mr-0 " ++ active}>
            ext->s
          </span>;
        },
      );

    let onClick = evt => {
      ReactEvent.Mouse.preventDefault(evt);

      // Rotate through the array
      let nextIdx =
        selectedIndex < Array.length(values) - 1 ? selectedIndex + 1 : 0;

      switch (Belt.Array.get(values, nextIdx)) {
      | Some(lang) => onChange(lang)
      | None => ()
      };
    };

    <button
      className={
        (disabled ? "opacity-25" : "")
        ++ " border border-night-light inline-block rounded px-4 py-1 flex text-16"
      }
      disabled
      onClick>
      elements->ate
    </button>;
  };
};

module Pane = {
  type tab = {
    title: string,
    content: React.element,
  };

  let defaultMakeTabClass = (active: bool): string => {
    let activeClass = active ? "text-fire font-medium bg-onyx" : "";

    "flex items-center h-12 px-4 pr-16 " ++ activeClass;
  };

  // tabClass: base class for bg color etc
  [@react.component]
  let make = (~tabs: array(tab), ~makeTabClass=defaultMakeTabClass) => {
    let (current, setCurrent) = React.useState(_ => 0);

    let headers =
      Belt.Array.mapWithIndex(
        tabs,
        (i, tab) => {
          let title = tab.title;
          let onClick = evt => {
            ReactEvent.Mouse.preventDefault(evt);
            setCurrent(_ => i);
          };
          let active = current === i;
          let className = makeTabClass(active);
          <div key={string_of_int(i) ++ "-" ++ title} onClick className>
            title->s
          </div>;
        },
      );

    let body =
      switch (Belt.Array.get(tabs, current)) {
      | Some(tab) => tab.content
      | None => React.null
      };

    <div>
      <div>
        <div className="flex bg-night-10 w-full"> headers->ate </div>
        <div> body </div>
      </div>
    </div>;
  };
};

module SingleTabPane = {
  [@react.component]
  let make = (~title: string, ~makeTabClass=?, ~children) => {
    let tabs = [|{Pane.title, content: children}|];

    <Pane tabs ?makeTabClass />;
  };
};

module ErrorPane = {
  module ActionIndicator = {
    [@react.component]
    let make = () => {
      <div className="animate-pulse"> "<!>"->s </div>;
    };
  };

  module PreWrap = {
    [@react.component]
    let make = (~className="", ~children) => {
      <pre className={"whitespace-pre-wrap " ++ className}> children </pre>;
    };
  };
  type prefix = [ | `W | `E];
  let compactErrorLine =
      (~highlight=false, ~prefix: prefix, locMsg: Api.LocMsg.t) => {
    let {Api.LocMsg.row, column, shortMsg} = locMsg;
    let prefixColor =
      switch (prefix) {
      | `W => "text-code-5"
      | `E => "text-fire"
      };

    let prefixText =
      switch (prefix) {
      | `W => "[W]"
      | `E => "[E]"
      };

    let highlightClass =
      switch (highlight, prefix) {
      | (false, _) => ""
      | (true, `W) => "bg-gold-15"
      | (true, `E) => "bg-fire-15 rounded"
      };

    <div
      className="font-mono mb-4 pb-6 last:mb-0 last:pb-0 last:border-0 border-b border-night-light ">
      <div className={"p-2 " ++ highlightClass}>
        <span className=prefixColor> prefixText->s </span>
        <span className="font-medium text-night-light">
          {j| Line $row, column $column:|j}->s
        </span>
        <AnsiPre className="whitespace-pre-wrap "> shortMsg </AnsiPre>
      </div>
    </div>;
  };

  let isHighlighted = (~focusedRowCol=?, locMsg): bool => {
    switch (focusedRowCol) {
    | Some(focusedRowCol) =>
      let {Api.LocMsg.row, column} = locMsg;
      let (fRow, fCol) = focusedRowCol;

      fRow === row && fCol === column;

    | None => false
    };
  };

  let filterHighlightedLocMsgs =
      (~focusedRowCol, locMsgs: array(Api.LocMsg.t)): array(Api.LocMsg.t) => {
    Api.LocMsg.(
      switch (focusedRowCol) {
      | Some(focusedRowCol) =>
        let (fRow, fCol) = focusedRowCol;
        let filtered =
          Belt.Array.keep(locMsgs, locMsg => {
            fRow === locMsg.row && fCol === locMsg.column
          });

        if (Array.length(filtered) === 0) {
          locMsgs;
        } else {
          filtered;
        };

      | None => locMsgs
      }
    );
  };

  let filterHighlightedLocWarnings =
      (~focusedRowCol, warnings: array(Api.Warning.t)): array(Api.Warning.t) => {
    switch (focusedRowCol) {
    | Some(focusedRowCol) =>
      let (fRow, fCol) = focusedRowCol;
      let filtered =
        Belt.Array.keep(warnings, warning => {
          switch (warning) {
          | Warn({details})
          | WarnErr({details}) =>
            fRow === details.row && fCol === details.column
          }
        });
      if (Array.length(filtered) === 0) {
        warnings;
      } else {
        filtered;
      };
    | None => warnings
    };
  };

  let renderResult =
      (
        ~focusedRowCol: option((int, int)),
        ~targetLang: Api.Lang.t,
        ~compilerVersion: string,
        result: FinalResult.t,
      )
      : React.element => {
    switch (result) {
    | FinalResult.Comp(Fail(result)) =>
      switch (result) {
      | TypecheckErr(locMsgs)
      | OtherErr(locMsgs)
      | SyntaxErr(locMsgs) =>
        filterHighlightedLocMsgs(~focusedRowCol, locMsgs)
        ->Belt.Array.map(locMsg => {
            compactErrorLine(
              ~highlight=isHighlighted(~focusedRowCol?, locMsg),
              ~prefix=`E,
              locMsg,
            )
          })
        ->ate
      | WarningErr(warnings) =>
        filterHighlightedLocWarnings(~focusedRowCol, warnings)
        ->Belt.Array.map(warning => {
            let (prefix, details) =
              switch (warning) {
              | Api.Warning.Warn({details}) => (`W, details)
              | WarnErr({details}) => (`E, details)
              };
            compactErrorLine(
              ~highlight=isHighlighted(~focusedRowCol?, details),
              ~prefix,
              details,
            );
          })
        ->ate
      | WarningFlagErr({msg}) =>
        <div>
          "There are some issues with your compiler flag configuration:"->s
          msg->s
        </div>
      }
    | Comp(Success({warnings})) =>
      if (Array.length(warnings) === 0) {
        <PreWrap> "0 Errors, 0 Warnings"->s </PreWrap>;
      } else {
        filterHighlightedLocWarnings(~focusedRowCol, warnings)
        ->Belt.Array.map(warning => {
            let (prefix, details) =
              switch (warning) {
              | Api.Warning.Warn({details}) => (`W, details)
              | WarnErr({details}) => (`E, details)
              };
            compactErrorLine(
              ~highlight=isHighlighted(~focusedRowCol?, details),
              ~prefix,
              details,
            );
          })
        ->ate;
      }
    | Conv(Success({fromLang, toLang})) =>
      let msg =
        if (fromLang === toLang) {
          "Formatting completed with 0 errors";
        } else {
          let toStr = Api.Lang.toString(toLang);
          {j|Switched to $toStr with 0 errors|j};
        };
      <PreWrap> msg->s </PreWrap>;
    | Conv(Fail({fromLang, toLang, details})) =>
      let errs =
        filterHighlightedLocMsgs(~focusedRowCol, details)
        ->Belt.Array.map(locMsg => {
            compactErrorLine(
              ~highlight=isHighlighted(~focusedRowCol?, locMsg),
              ~prefix=`E,
              locMsg,
            )
          })
        ->ate;

      // The way the UI is currently designed, there shouldn't be a case where fromLang !== toLang.
      // We keep both cases though in case we change things later
      let msg =
        if (fromLang === toLang) {
          let langStr = Api.Lang.toString(toLang);
          {j|The code above is no valid $langStr syntax.|j};
        } else {
          let fromStr = Api.Lang.toString(fromLang);
          let toStr = Api.Lang.toString(toLang);
          {j|Could not convert from "$fromStr" to "$toStr" due to malformed syntax:|j};
        };
      <div> <PreWrap className="text-16 mb-4"> msg->s </PreWrap> errs </div>;
    | Comp(UnexpectedError(msg))
    | Conv(UnexpectedError(msg)) => msg->s
    | Comp(Unknown(msg, json))
    | Conv(Unknown(msg, json)) =>
      let subheader = "font-bold text-night-light text-16";
      <div>
        <PreWrap>
          "The compiler bundle API returned a result that couldn't be interpreted. Please open an issue on our "
          ->s
          <Markdown.A
            target="_blank"
            href="https://github.com/reason-association/reasonml.org/issues">
            "issue tracker"->s
          </Markdown.A>
          "."->s
        </PreWrap>
        <div className="mt-4">
          <PreWrap>
            <div className=subheader> "Message: "->s </div>
            msg->s
          </PreWrap>
        </div>
        <div className="mt-4">
          <PreWrap>
            <span className=subheader> "Received JSON payload:"->s </span>
            <div> {Util.Json.prettyStringify(json)->s} </div>
          </PreWrap>
        </div>
      </div>;
    | Nothing =>
      let syntax =
        switch (targetLang) {
        | Api.Lang.OCaml => "OCaml"
        | Res => "New BuckleScript"
        | Reason => "Reason"
        };
      <PreWrap>
        {j|This playground is now running on compiler version $compilerVersion with the $syntax syntax|j}
        ->s
      </PreWrap>;
    };
  };

  let renderTitle = result => {
    let errClass = "text-fire";
    let warnClass = "text-code-5";
    let okClass = "text-dark-code-3";

    let (className, text) =
      switch (result) {
      | FinalResult.Comp(Fail(result)) =>
        switch (result) {
        | SyntaxErr(_) => (errClass, "Syntax Errors")
        | TypecheckErr(_) => (errClass, "Type Errors")
        | WarningErr(_) => (warnClass, "Warning Errors")
        | WarningFlagErr(_) => (errClass, "Config Error")
        | OtherErr(_) => (errClass, "Errors")
        }
      | Conv(Fail(_)) => (errClass, "Syntax Errors")
      | Comp(Success({warnings})) =>
        if (Belt.Array.length(warnings) === 0) {
          (okClass, "Compilation Successful");
        } else {
          (warnClass, "Success with Warnings");
        }
      | Conv(Success(_)) => (okClass, "Format Successful")
      | Comp(UnexpectedError(_))
      | Conv(UnexpectedError(_)) => (errClass, "Unexpected Error")
      | Comp(Unknown(_))
      | Conv(Unknown(_)) => (errClass, "Unknown Result")
      | Nothing => (okClass, "Ready")
      };

    <span className> text->s </span>;
  };

  [@react.component]
  let make =
      (
        ~actionIndicatorKey: string,
        ~targetLang: Api.Lang.t,
        ~compilerVersion: string,
        ~focusedRowCol: option((int, int))=?,
        ~result: FinalResult.t,
      ) => {
    let activityIndicatorColor =
      switch (result) {
      | FinalResult.Comp(Fail(_))
      | Conv(Fail(_))
      | Comp(UnexpectedError(_))
      | Conv(UnexpectedError(_))
      | Comp(Unknown(_))
      | Conv(Unknown(_)) => "bg-fire-80"
      | Conv(Success(_))
      | Nothing => "bg-dark-code-3"
      | Comp(Success({warnings})) =>
        if (Array.length(warnings) === 0) {
          "bg-dark-code-3";
        } else {
          "bg-code-5";
        }
      };

    <div
      className="pt-4 bg-night-dark overflow-y-auto hide-scrollbar"
      style={ReactDOMRe.Style.make(
        ~minHeight="20rem",
        ~maxHeight="20rem",
        (),
      )}>
      <div className="flex items-center text-16 font-medium px-4">
        <div className="pr-4"> {renderTitle(result)} </div>
        <div
          key=actionIndicatorKey
          className={
            "animate-pulse block h-1 w-1 rounded-full "
            ++ activityIndicatorColor
          }
        />
      </div>
      <div className="">
        <div className="bg-night-dark text-snow-darker px-4 py-4">
          {renderResult(~focusedRowCol, ~compilerVersion, ~targetLang, result)}
        </div>
      </div>
    </div>;
  };
};

module ControlPanel = {
  [@react.component]
  let make =
      (
        ~isCompilerSwitching: bool,
        ~langSelectionDisabled: bool, // In case a syntax conversion error occurred
        ~compilerVersion: string,
        ~availableCompilerVersions: array(string),
        ~availableTargetLangs: array(Api.Lang.t),
        ~selectedTargetLang: (Api.Lang.t, string), // (lang, version)
        ~loadedLibraries: array(string),
        ~onCompilerSelect: string => unit,
        ~onFormatClick: option(unit => unit)=?,
        ~formatDisabled: bool=false,
        ~onCompileClick: unit => unit,
        ~onTargetLangSelect: Api.Lang.t => unit,
      ) => {
    let (targetLang, targetLangVersion) = selectedTargetLang;

    let targetLangName = Api.Lang.toString(targetLang);

    let formatClickHandler =
      switch (onFormatClick) {
      | Some(cb) =>
        let handler = evt => {
          ReactEvent.Mouse.preventDefault(evt);
          cb();
        };
        Some(handler);
      | None => None
      };

    <div className="flex bg-onyx text-night-light px-6 text-14 w-full">
      <div
        className="flex justify-between items-center border-t py-4 border-night-60 w-full">
        <div>
          <span className="font-semibold mr-2"> "BS"->s </span>
          <DropdownSelect
            name="compilerVersions"
            value=compilerVersion
            disabled=isCompilerSwitching
            onChange={evt => {
              ReactEvent.Form.preventDefault(evt);
              let id = evt->ReactEvent.Form.target##value;
              onCompilerSelect(id);
            }}>
            {Belt.Array.map(availableCompilerVersions, version => {
               <option className="py-4" key=version value=version>
                 version->s
               </option>
             })
             ->ate}
          </DropdownSelect>
        </div>
        <LanguageToggle
          values=availableTargetLangs
          selected=targetLang
          disabled=isCompilerSwitching
          onChange=onTargetLangSelect
        />
        <button
          className={
            (isCompilerSwitching ? "opacity-25" : "")
            ++ " font-semibold inline-block border border-night-light rounded py-1 px-4"
          }
          disabled=isCompilerSwitching
          onClick=?formatClickHandler>
          "Format"->s
        </button>
        /*
         <div className="font-semibold">
           "Bindings: "->s
           {switch (loadedLibraries) {
            | [||] => "No third party library loaded"->s
            | arr => Js.Array2.joinWith(arr, ", ")->s
            }}
         </div>
         */
        <button
          disabled=isCompilerSwitching
          className={
            (isCompilerSwitching ? "opacity-25" : "")
            ++ " inline-block bg-sky text-16 text-white-80 rounded py-2 px-6"
          }
          onClick={evt => {
            ReactEvent.Mouse.preventDefault(evt);
            onCompileClick();
          }}>
          "Compile"->s
        </button>
      </div>
    </div>;
  };
};

let locMsgToCmError =
    (~kind: CodeMirrorBase.Error.kind, locMsg: Api.LocMsg.t)
    : CodeMirrorBase.Error.t => {
  let {Api.LocMsg.row, column, endColumn, endRow, shortMsg} = locMsg;
  {CodeMirrorBase.Error.row, column, endColumn, endRow, text: shortMsg, kind};
};

[@react.component]
let default = () => {
  // We don't count to infinity. This value is only required to trigger
  // rerenders for specific components (ActivityIndicator)
  let (actionCount, setActionCount) = React.useState(_ => 0);
  let onAction = _ => {
    setActionCount(prev => prev > 1000000 ? 0 : prev + 1);
  };
  let (compilerState, compilerDispatch) = useCompilerManager(~onAction, ());

  let overlayState = React.useState(() => false);

  // The user can focus an error / warning on a specific line & column
  // which is stored in this ref and triggered by hover / click states
  // in the CodeMirror editor
  let (focusedRowCol, setFocusedRowCol) = React.useState(_ => None);

  let initialContent = {j|
  let a = 1 + "";
module Test2 = {
  [@react.component]
  let make = (~a: string, ~b: string) => {
    <div>
    </div>
  };
}|j};

  let initialContent = {j|module A = {
  let = 1;
  let a = 1;
}

module B = {
  let = 2
  let b = 2
}|j};

  let jsOutput =
    Api.(
      switch (compilerState) {
      | Init => "/* Initializing Playground... */"
      | Ready({result: FinalResult.Comp(comp)}) =>
        switch (comp) {
        | CompilationResult.Success({js_code}) => js_code
        | UnexpectedError(msg)
        | Unknown(msg, _) => {j|/* Unexpected Result: $msg */|j}
        | Fail(_) => "/* Could not compile, check the error pane for details. */"
        }
      | Ready({result: Nothing})
      | Ready({result: Conv(_)}) => "/* Compiler ready! Press the \"Compile\" button to see the JS output. */"
      | Compiling(_, _) => "/* Compiling... */"
      | SwitchingCompiler(_, version, libraries) =>
        let appendix =
          if (Js.Array.length(libraries) > 0) {
            " (+" ++ Js.Array2.joinWith(libraries, ", ") ++ ")";
          } else {
            "";
          };
        "/* Switching to " ++ version ++ appendix ++ " ... */";
      | _ => ""
      }
    );

  let isReady =
    switch (compilerState) {
    | Ready(_) => true
    | _ => false
    };

  let editorCode = React.useRef(initialContent);

  /* In case the compiler did some kind of syntax conversion / reformatting,
     we take any success results and set the editor code to the new formatted code */
  switch (compilerState) {
  | Ready({result: FinalResult.Conv(Api.ConversionResult.Success({code}))}) =>
    React.Ref.setCurrent(editorCode, code)
  | _ => ()
  };

  Js.log2("state", compilerState);

  let cmErrors =
    switch (compilerState) {
    | Ready({result}) =>
      switch (result) {
      | FinalResult.Comp(Fail(result)) =>
        switch (result) {
        | SyntaxErr(locMsgs)
        | TypecheckErr(locMsgs)
        | OtherErr(locMsgs) =>
          Belt.Array.map(locMsgs, locMsgToCmError(~kind=`Error))
        | WarningErr(warnings) =>
          Belt.Array.reduce(
            warnings,
            [||],
            (acc, next) => {
              switch (next) {
              | Api.Warning.Warn({details})
              | WarnErr({details}) =>
                let warn = locMsgToCmError(~kind=`Warning, details);
                Js.Array2.push(acc, warn)->ignore;
              };
              acc;
            },
          )
        | WarningFlagErr(_) => [||]
        }
      | Comp(Success({warnings})) =>
        Belt.Array.reduce(
          warnings,
          [||],
          (acc, next) => {
            switch (next) {
            | Api.Warning.Warn({details})
            | WarnErr({details}) =>
              let warn = locMsgToCmError(~kind=`Warning, details);
              Js.Array2.push(acc, warn)->ignore;
            };
            acc;
          },
        )
      | Conv(Fail({details})) =>
        Belt.Array.map(details, locMsgToCmError(~kind=`Error))
      | Comp(_)
      | Conv(_)
      | Nothing => [||]
      }
    | _ => [||]
    };

  <>
    <Meta
      title="Reason Playground"
      description="Try ReasonML in the browser"
    />
    <div className="text-16 mb-32 mt-16 pt-2 bg-night-dark">
      <div className="text-night text-lg">
        <Navigation overlayState />
        <main className="mx-10 mt-16 pb-32 flex justify-center">
          <div className="flex max-w-1280 w-full border-4 border-night">
            <div className="w-full max-w-705 border-r-4 border-night">
              <SingleTabPane title="ReasonML">
                <div className="bg-onyx text-snow-darker">
                  <CodeMirror
                    className="w-full"
                    minHeight="40vh"
                    mode="reason"
                    errors=cmErrors
                    value={React.Ref.current(editorCode)}
                    onChange={value => {
                      React.Ref.setCurrent(editorCode, value)
                    }}
                    onMarkerFocus={rowCol => {
                      setFocusedRowCol(prev => {Some(rowCol)})
                    }}
                    onMarkerFocusLeave={_ => {setFocusedRowCol(_ => None)}}
                  />
                </div>
              </SingleTabPane>
              {switch (compilerState) {
               | Ready(ready)
               | Compiling(ready, _)
               | SwitchingCompiler(ready, _, _) =>
                 let availableTargetLangs =
                   Api.Version.availableLanguages(ready.selected.apiVersion);

                 let selectedTargetLang =
                   switch (ready.targetLang) {
                   | Res => (Api.Lang.Res, ready.selected.compilerVersion)
                   | Reason => (Reason, ready.selected.reasonVersion)
                   | OCaml => (OCaml, ready.selected.ocamlVersion)
                   };

                 let onCompilerSelect = id => {
                   compilerDispatch(
                     SwitchToCompiler({
                       id,
                       libraries: ready.selected.libraries,
                     }),
                   );
                 };

                 let onTargetLangSelect = lang => {
                   compilerDispatch(
                     SwitchLanguage({
                       lang,
                       code: React.Ref.current(editorCode),
                     }),
                   );
                 };

                 let onCompileClick = () => {
                   compilerDispatch(
                     CompileCode(
                       ready.targetLang,
                       React.Ref.current(editorCode),
                     ),
                   );
                 };

                 // When a new compiler version was selected, it should
                 // be shown in the control panel as the currently selected
                 // version, even when it is currently loading
                 let compilerVersion =
                   switch (compilerState) {
                   | SwitchingCompiler(_, version, _) => version
                   | _ => ready.selected.id
                   };

                 let langSelectionDisabled =
                   Api.(
                     switch (ready.result) {
                     | FinalResult.Conv(ConversionResult.Fail(_))
                     | Comp(CompilationResult.Fail(_)) => true
                     | _ => false
                     }
                   );

                 let onFormatClick = () => {
                   compilerDispatch(Format(React.Ref.current(editorCode)));
                 };

                 let actionIndicatorKey = string_of_int(actionCount);

                 let isCompilerSwitching =
                   switch (compilerState) {
                   | SwitchingCompiler(_, _, _) => true
                   | _ => false
                   };
                 <>
                   <ControlPanel
                     isCompilerSwitching
                     langSelectionDisabled
                     compilerVersion
                     availableTargetLangs
                     availableCompilerVersions={ready.versions}
                     selectedTargetLang
                     loadedLibraries={ready.selected.libraries}
                     onCompilerSelect
                     onTargetLangSelect
                     onCompileClick
                     onFormatClick
                   />
                   <div className="border-fire border-t-4">
                     <ErrorPane
                       actionIndicatorKey
                       targetLang={ready.targetLang}
                       compilerVersion={ready.selected.compilerVersion}
                       ?focusedRowCol
                       result={ready.result}
                     />
                   </div>
                 </>;
               | Init => "Initializing"->s
               | SetupFailed(msg) =>
                 let content = ("Setup failed: " ++ msg)->s;
                 let tabs = [|{Pane.title: "Error", content}|];
                 <> <Pane tabs /> </>;
               }}
            </div>
            <SingleTabPane
              title="JavaScript"
              makeTabClass={active => {
                let activeClass =
                  active ? "text-fire font-medium bg-night-dark" : "";

                "flex items-center h-12 px-4 pr-16 " ++ activeClass;
              }}>
              <div className="w-full bg-night-dark text-snow-darker">
                <CodeMirror
                  className="w-full"
                  minHeight="40vh"
                  mode="javascript"
                  lineWrapping=true
                  value=jsOutput
                  readOnly=true
                />
              </div>
            </SingleTabPane>
          </div>
        </main>
      </div>
    </div>
  </>;
};
