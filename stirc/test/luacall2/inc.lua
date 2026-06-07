function luafun(dummy)
  local res=Abce.makelexcall("AMYPLANDUMP", {1,2,{a=3, b=4}})
  print(res[2])
end
